#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

#include "SparkFlex.hpp"

/*
 * Test: does the Spark Flex accept velocity via SetCtrlType + SetSetpoint
 * instead of the Spark MAX-style SetVelocity (API class 1)?
 *
 * Build:
 *   /usr/bin/g++ -std=c++17 -Iinclude -o flex_velocity_test \
 *       examples/flex_velocity_test.cpp src/SparkBase.cpp src/SparkFlex.cpp -lpthread
 * Run:
 *   sudo ./flex_velocity_test [can_id]
 */

template <typename Fn>
static void run_phase(SparkFlex & motor, const char * label, int seconds, Fn command)
{
  std::cout << "\n=== " << label << " (" << seconds << "s) ===" << std::endl;
  auto start = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
    if (elapsed.count() >= seconds * 1000) break;

    motor.Heartbeat();
    command();

    std::cout << std::fixed << std::setprecision(2);
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

    // --- Phase 1: DutyCycle baseline ---
    run_phase(motor, "Phase 1: DutyCycle 0.2 (baseline)", 5, [&]() {
      motor.SetDutyCycle(0.2f);
    });

    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 2: SetCtrlType(kVelocity) + PID + SetSetpoint(500) ---
    std::cout << "\nConfiguring velocity mode via CtrlType + Setpoint..." << std::endl;
    float kF = 1.0f / 5700.0f;
    float kP = 0.0002f;
    motor.SetP(0, kP);
    motor.SetI(0, 0.0f);
    motor.SetD(0, 0.0f);
    motor.SetF(0, kF);
    motor.SetCtrlType(CtrlType::kVelocity);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "PID: kP=" << kP << " kF=" << kF << "  CtrlType=Velocity" << std::endl;

    run_phase(motor, "Phase 2: CtrlType=Velocity + SetSetpoint(500)", 5, [&]() {
      motor.SetSetpoint(500.0f);
    });

    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 3: Higher RPM via SetSetpoint ---
    run_phase(motor, "Phase 3: CtrlType=Velocity + SetSetpoint(2000)", 5, [&]() {
      motor.SetSetpoint(2000.0f);
    });

    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 4: Switch back to DutyCycle to confirm motor still responds ---
    motor.SetCtrlType(CtrlType::kDutyCycle);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    run_phase(motor, "Phase 4: Back to DutyCycle 0.2 (confirm recovery)", 3, [&]() {
      motor.SetDutyCycle(0.2f);
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
