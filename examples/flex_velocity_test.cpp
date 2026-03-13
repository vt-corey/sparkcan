#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Raw CAN brute-force: find the byte layout for Spark Flex velocity control.
 *
 * Build:
 *   /usr/bin/g++ -std=c++17 -o flex_velocity_test examples/flex_velocity_test.cpp -lpthread
 * Run:
 *   sudo ./flex_velocity_test [can_id]
 */

static uint32_t make_arb_id(uint8_t dev_type, uint8_t mfg,
                            uint8_t api_class, uint8_t api_index, uint8_t dev_id) {
  return (uint32_t(dev_type) << 24) | (uint32_t(mfg) << 16) |
         (uint32_t(api_class) << 10) | (uint32_t(api_index) << 6) | dev_id;
}

static int open_can(const char * iface) {
  int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (s < 0) { perror("socket"); return -1; }
  struct ifreq ifr;
  std::strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
  if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { perror("ioctl"); close(s); return -1; }
  struct sockaddr_can addr = {};
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(s); return -1; }
  return s;
}

static void send_frame(int sock, uint32_t arb_id, const uint8_t data[8]) {
  struct can_frame frame = {};
  frame.can_id = arb_id | CAN_EFF_FLAG;
  frame.can_dlc = 8;
  std::memcpy(frame.data, data, 8);
  write(sock, &frame, sizeof(frame));
}

// Heartbeat = apiClass=11, apiIndex=2
static void send_heartbeat(int sock, uint8_t dev_id) {
  uint32_t hb_id = make_arb_id(0x02, 0x05, 11, 2, dev_id);
  uint8_t hb_data[8] = {};
  send_frame(sock, hb_id, hb_data);
}

static void run_test(int sock, uint8_t dev_id, const char * label, int seconds,
                     const uint8_t data[8], uint32_t arb_id) {
  std::cout << "\n=== " << label << " (" << seconds << "s) ===" << std::endl;
  std::cout << "  ArbId=0x" << std::hex << arb_id << std::dec << "  data: ";
  for (int b = 0; b < 8; b++)
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[b] << " ";
  std::cout << std::dec << std::endl;

  auto start = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
    if (elapsed.count() >= seconds * 1000) break;

    send_heartbeat(sock, dev_id);
    send_frame(sock, arb_id, data);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::cout << "  >> Did the motor move?" << std::endl;
}

static void stop_motor(int sock, uint8_t dev_id) {
  uint32_t duty_id = make_arb_id(0x02, 0x05, 0, 2, dev_id);
  float zero = 0.0f;
  uint8_t data[8] = {};
  std::memcpy(data, &zero, 4);
  for (int i = 0; i < 10; i++) {
    send_heartbeat(sock, dev_id);
    send_frame(sock, duty_id, data);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

int main(int argc, char * argv[])
{
  uint8_t dev_id = 1;
  if (argc > 1) dev_id = std::atoi(argv[1]);

  int sock = open_can("can0");
  if (sock < 0) return 1;

  uint32_t duty_arb = make_arb_id(0x02, 0x05, 0, 2, dev_id);  // DutyCycle
  uint32_t vel_arb  = make_arb_id(0x02, 0x05, 1, 2, dev_id);   // Velocity
  uint32_t sp_arb   = make_arb_id(0x02, 0x05, 0, 1, dev_id);   // Setpoint

  // ---- Phase 0: DutyCycle baseline ----
  {
    float duty = 0.2f;
    uint8_t d[8] = {};
    std::memcpy(d, &duty, 4);
    run_test(sock, dev_id, "Baseline: DutyCycle(0.2) on DutyCycle ArbId", 3, d, duty_arb);
    stop_motor(sock, dev_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Velocity setpoint
  float vel = 500.0f;
  uint8_t vb[4];
  std::memcpy(vb, &vel, 4);

  // ---- Try ctrl=1 (kVelocity) in each byte on DutyCycle ArbId ----
  struct { const char * name; uint8_t d[8]; uint32_t arb; } tests[] = {
    {"DutyArb: ctrl=1 @ byte4",       {vb[0],vb[1],vb[2],vb[3], 1, 0, 0, 0}, duty_arb},
    {"DutyArb: ctrl=1 @ byte5",       {vb[0],vb[1],vb[2],vb[3], 0, 1, 0, 0}, duty_arb},
    {"DutyArb: ctrl=1 @ byte6",       {vb[0],vb[1],vb[2],vb[3], 0, 0, 1, 0}, duty_arb},
    {"DutyArb: ctrl=1 @ byte7",       {vb[0],vb[1],vb[2],vb[3], 0, 0, 0, 1}, duty_arb},
    {"DutyArb: ctrl=1 @ b6 hi-nib",   {vb[0],vb[1],vb[2],vb[3], 0, 0, 0x10, 0}, duty_arb},
    {"DutyArb: ctrl=1 @ b4 hi-nib",   {vb[0],vb[1],vb[2],vb[3], 0x10, 0, 0, 0}, duty_arb},
    // Try on Velocity ArbId with ctrl in data
    {"VelArb: ctrl=1 @ byte4",        {vb[0],vb[1],vb[2],vb[3], 1, 0, 0, 0}, vel_arb},
    // Try on Setpoint ArbId with ctrl in data
    {"SetpArb: ctrl=1 @ byte4",       {vb[0],vb[1],vb[2],vb[3], 1, 0, 0, 0}, sp_arb},
    {"SetpArb: ctrl=1 @ byte6",       {vb[0],vb[1],vb[2],vb[3], 0, 0, 1, 0}, sp_arb},
  };

  int n = sizeof(tests) / sizeof(tests[0]);
  for (int i = 0; i < n; i++) {
    run_test(sock, dev_id, tests[i].name, 3, tests[i].d, tests[i].arb);
    stop_motor(sock, dev_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "\nAll tests done." << std::endl;
  stop_motor(sock, dev_id);
  close(sock);
  return 0;
}
