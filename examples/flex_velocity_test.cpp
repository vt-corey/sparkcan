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

// REV CAN arbitration ID format
static uint32_t make_arb_id(uint8_t device_type, uint8_t manufacturer,
                            uint8_t api_class, uint8_t api_index, uint8_t device_id) {
  return (uint32_t(device_type) << 24) | (uint32_t(manufacturer) << 16) |
         (uint32_t(api_class) << 10) | (uint32_t(api_index) << 6) | device_id;
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

static void run_test(int sock, uint8_t dev_id, const char * label, int seconds,
                     const uint8_t data[8], uint32_t arb_id) {
  // Heartbeat arb id: API class 0, index 0 (Heartbeat in sparkcan)
  uint32_t hb_id = make_arb_id(0x02, 0x05, 0, 0, dev_id);
  uint8_t hb_data[8] = {};

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

    send_frame(sock, hb_id, hb_data);
    send_frame(sock, arb_id, data);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::cout << "  >> Check if motor moved!" << std::endl;
}

static void stop_motor(int sock, uint8_t dev_id) {
  uint32_t duty_id = make_arb_id(0x02, 0x05, 0, 2, dev_id);
  float zero = 0.0f;
  uint8_t data[8] = {};
  std::memcpy(data, &zero, 4);
  for (int i = 0; i < 10; i++) {
    uint32_t hb_id = make_arb_id(0x02, 0x05, 0, 0, dev_id);
    uint8_t hb[8] = {};
    send_frame(sock, hb_id, hb);
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

  // DutyCycle ArbId: class=0, index=2
  uint32_t duty_arb = make_arb_id(0x02, 0x05, 0, 2, dev_id);

  // ---- Phase 0: DutyCycle baseline ----
  {
    float duty = 0.2f;
    uint8_t data[8] = {};
    std::memcpy(data, &duty, 4);
    run_test(sock, dev_id, "Baseline: DutyCycle(0.2)", 3, data, duty_arb);
    stop_motor(sock, dev_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // ---- Velocity tests: send to DutyCycle ArbId with ctrl type in data ----
  float vel = 500.0f;
  uint8_t vb[4];
  std::memcpy(vb, &vel, 4);

  // Feedforward for 500 RPM: 500/5700 ≈ 0.0877 duty, as 12-bit: 0.0877*1024 ≈ 90
  // But let's first try with no feedforward, just ctrl type

  struct { const char * name; uint8_t d[8]; } tests[] = {
    // Try ctrl=1 in each byte position
    {"vel=500 ctrl=1 @ byte4",       {vb[0],vb[1],vb[2],vb[3], 1, 0, 0, 0}},
    {"vel=500 ctrl=1 @ byte5",       {vb[0],vb[1],vb[2],vb[3], 0, 1, 0, 0}},
    {"vel=500 ctrl=1 @ byte6",       {vb[0],vb[1],vb[2],vb[3], 0, 0, 1, 0}},
    {"vel=500 ctrl=1 @ byte7",       {vb[0],vb[1],vb[2],vb[3], 0, 0, 0, 1}},
    // Try ctrl=1 in upper/lower nibbles
    {"vel=500 ctrl=1 @ byte6[7:4]",  {vb[0],vb[1],vb[2],vb[3], 0, 0, 0x10, 0}},
    {"vel=500 ctrl=1 @ byte4[7:4]",  {vb[0],vb[1],vb[2],vb[3], 0x10, 0, 0, 0}},
    // Try ctrl in byte6 with pidSlot=0 packed
    {"vel=500 b6=(ctrl<<4|slot)",    {vb[0],vb[1],vb[2],vb[3], 0, 0, 0x10, 0}},
    // Also try sending to the Velocity ArbId (class=1, index=2) with ctrl in data
    // Maybe the Flex needs BOTH the right ArbId AND ctrl in data?
  };

  int n = sizeof(tests) / sizeof(tests[0]);
  for (int i = 0; i < n; i++) {
    run_test(sock, dev_id, tests[i].name, 3, tests[i].d, duty_arb);
    stop_motor(sock, dev_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Also try Velocity ArbId (class=1, index=2) with ctrl=1 in data
  uint32_t vel_arb = make_arb_id(0x02, 0x05, 1, 2, dev_id);
  {
    uint8_t d[8] = {vb[0],vb[1],vb[2],vb[3], 1, 0, 0, 0};
    run_test(sock, dev_id, "Velocity ArbId + ctrl=1 @ byte4", 3, d, vel_arb);
    stop_motor(sock, dev_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Try Setpoint ArbId (class=0, index=1) with ctrl=1 in various positions
  uint32_t sp_arb = make_arb_id(0x02, 0x05, 0, 1, dev_id);
  {
    uint8_t d[8] = {vb[0],vb[1],vb[2],vb[3], 1, 0, 0, 0};
    run_test(sock, dev_id, "Setpoint ArbId + ctrl=1 @ byte4", 3, d, sp_arb);
    stop_motor(sock, dev_id);
  }

  std::cout << "\nAll tests done." << std::endl;
  stop_motor(sock, dev_id);
  close(sock);
  return 0;
}
