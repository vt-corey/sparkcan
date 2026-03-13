#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <thread>

#include "SparkFlex.hpp"

/*
 * Standalone velocity-control test for Spark Flex + Neo Vortex.
 *
 * Build on the Pi:
 *   cd ~/ros-turtle/sparkcan
 *   g++ -std=c++17 -Iinclude -o flex_velocity_test \
 *       examples/flex_velocity_test.cpp src/SparkBase.cpp src/SparkFlex.cpp \
 *       -lpthread
 *
 * Run:
 *   sudo ./flex_velocity_test [can_id]     # default can_id=1
 *
 * The test runs 4 phases (5 s each):
 *   1) Duty cycle 20% — baseline sanity check
 *   2) Velocity 500 RPM with kF only (no kP)
 *   3) Velocity 500 RPM with kF + kP
 *   4) Velocity 2000 RPM with kF + kP
 * Then stops the motor.
 */

static void run_phase(SparkFlex & motor, const char * label, int seconds,
                      std::function<void()> command)
{
  std::cout << "\n=== " << label << " (" << seconds << "s) ===" << std::endl;
  auto start = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
    if (elapsed.count() >= seconds * 1000) break;

    motor.Heartbeat();
    command();

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "\r  t=" << (elapsed.count() / 1000.0) << "s"
              << "  vel=" << motor.GetVelocity() << " RPM"
              << "  V=" << motor.GetVoltage() << "V"
              << "  I=" << motor.GetCurrent() << "A"
              << "  T=" << motor.GetTemperature() << "C"
              << "        " << std::flush;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::cout << std::endl;
}

int main(int argc, char * argv[])
{
  int can_id = 1;
  if (argc > 1) can_id = std::atoi(argv[1]);

  try {
    std::cout << "Opening SparkFlex on can0, ID " << can_id << std::endl;
    SparkFlex motor("can0", static_cast<uint8_t>(can_id));

    motor.ResetFaults();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Phase 1: duty cycle baseline ---
    run_phase(motor, "Phase 1: DutyCycle 0.2 (sanity check)", 5, [&]() {
      motor.SetDutyCycle(0.2f);
    });

    // Stop briefly between phases
    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 2: velocity with feedforward only ---
    float kF = 1.0f / 5700.0f;
    std::cout << "\nSetting PID: kP=0  kI=0  kD=0  kF=" << kF << std::endl;
    motor.SetP(0, 0.0f);
    motor.SetI(0, 0.0f);
    motor.SetD(0, 0.0f);
    motor.SetF(0, kF);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    run_phase(motor, "Phase 2: SetVelocity(500) kF only", 5, [&]() {
      motor.SetVelocity(500.0f);
    });

    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 3: velocity with kF + kP ---
    float kP = 0.0002f;
    std::cout << "\nSetting PID: kP=" << kP << "  kI=0  kD=0  kF=" << kF << std::endl;
    motor.SetP(0, kP);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    run_phase(motor, "Phase 3: SetVelocity(500) kF+kP", 5, [&]() {
      motor.SetVelocity(500.0f);
    });

    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 4: higher velocity ---
    run_phase(motor, "Phase 4: SetVelocity(2000) kF+kP", 5, [&]() {
      motor.SetVelocity(2000.0f);
    });

    // --- Stop ---
    std::cout << "\nStopping motor..." << std::endl;
    motor.SetDutyCycle(0.0f);
    motor.Heartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "Done." << std::endl;

  } catch (const std::exception & e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
