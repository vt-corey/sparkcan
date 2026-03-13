#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <functional>

#include "SparkFlex.hpp"

/*
 * Velocity control test using the sparkcan library directly.
 *
 * Build (on Pi):
 *   /usr/bin/g++ -std=c++17 -O2 -I include -o velocity_ctrl_test \
 *     examples/velocity_ctrl_test.cpp src/SparkBase.cpp src/SparkFlex.cpp -lpthread
 *
 * Run:
 *   sudo ./velocity_ctrl_test [can_id]
 */

static void run_phase(SparkFlex & motor, const char * label, int seconds,
                      std::function<void()> command_fn)
{
  std::cout << "\n=== " << label << " (" << seconds << "s) ===" << std::endl;

  auto start = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
    if (elapsed.count() >= seconds * 1000) break;

    motor.Heartbeat();
    command_fn();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  // Stop
  for (int i = 0; i < 10; i++) {
    motor.Heartbeat();
    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::cout << "  >> Did the motor move?" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main(int argc, char * argv[])
{
  uint8_t dev_id = 1;
  if (argc > 1) dev_id = std::atoi(argv[1]);

  std::cout << "Velocity control test for device " << (int)dev_id << std::endl;

  try {
    SparkFlex motor("can0", dev_id);

    // Phase 0: DutyCycle baseline — this MUST work
    run_phase(motor, "Phase 0: DutyCycle(0.2) baseline", 3, [&]() {
      motor.SetDutyCycle(0.2f);
    });

    // Phase 1: ctrl=1 in byte 4
    run_phase(motor, "Phase 1: vel=500 ctrl=1 @ byte4", 3, [&]() {
      motor.SendSetpointWithCtrlType(500.0f, 1, 4);
    });

    // Phase 2: ctrl=1 in byte 5
    run_phase(motor, "Phase 2: vel=500 ctrl=1 @ byte5", 3, [&]() {
      motor.SendSetpointWithCtrlType(500.0f, 1, 5);
    });

    // Phase 3: ctrl=1 in byte 6
    run_phase(motor, "Phase 3: vel=500 ctrl=1 @ byte6", 3, [&]() {
      motor.SendSetpointWithCtrlType(500.0f, 1, 6);
    });

    // Phase 4: ctrl=1 in byte 7
    run_phase(motor, "Phase 4: vel=500 ctrl=1 @ byte7", 3, [&]() {
      motor.SendSetpointWithCtrlType(500.0f, 1, 7);
    });

    // Phase 5: lower velocity (100 RPM) with ctrl in byte 6
    run_phase(motor, "Phase 5: vel=100 ctrl=1 @ byte6", 3, [&]() {
      motor.SendSetpointWithCtrlType(100.0f, 1, 6);
    });

    // Phase 6: sparkcan's existing SetVelocity (API class 1 in arb ID)
    run_phase(motor, "Phase 6: SetVelocity(500) arb-ID method", 3, [&]() {
      motor.SetVelocity(500.0f);
    });

    std::cout << "\nAll phases complete." << std::endl;

    for (int i = 0; i < 20; i++) {
      motor.Heartbeat();
      motor.SetDutyCycle(0.0f);
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

  } catch (const std::exception & e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
