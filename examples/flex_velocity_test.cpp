#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

#include "SparkFlex.hpp"

/*
 * Diagnostic test: which control commands does the Spark Flex actually accept?
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
    run_phase(motor, "Phase 1: SetDutyCycle(0.2)", 4, [&]() {
      motor.SetDutyCycle(0.2f);
    });
    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 2: SetSetpoint(0.2) without changing CtrlType ---
    // If CtrlType defaults to DutyCycle, SetSetpoint(0.2) should behave like DutyCycle(0.2)
    run_phase(motor, "Phase 2: SetSetpoint(0.2) [default CtrlType]", 4, [&]() {
      motor.SetSetpoint(0.2f);
    });
    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 3: SetVoltage(2.0) — open-loop voltage, no PID needed ---
    run_phase(motor, "Phase 3: SetVoltage(2.0)", 4, [&]() {
      motor.SetVoltage(2.0f);
    });
    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // --- Phase 4: Read back parameters to check if writes work ---
    std::cout << "\n=== Phase 4: Parameter read/write test ===" << std::endl;
    std::cout << "Writing kP=0.12345 to slot 0..." << std::endl;
    motor.SetP(0, 0.12345f);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto kp_read = motor.GetP(0);
    std::cout << "Read back kP slot 0: " << kp_read << std::endl;

    auto ctrl_type = motor.GetCtrlType();
    std::cout << "Current CtrlType: " << static_cast<int>(ctrl_type) << " (0=Duty,1=Vel,2=Volt,3=Pos)" << std::endl;

    auto feedback_sensor = motor.GetFeedbackSensorPID0();
    std::cout << "FeedbackSensorPID0: " << feedback_sensor << std::endl;

    // --- Phase 5: Configure feedback sensor + CtrlType + SetSetpoint ---
    std::cout << "\n=== Phase 5: Full config: FeedbackSensor=1(Hall) + CtrlType=Velocity ===" << std::endl;
    motor.SetFeedbackSensorPID0(1);  // 1 = Hall sensor
    motor.SetCtrlType(CtrlType::kVelocity);
    motor.SetP(0, 0.0002f);
    motor.SetI(0, 0.0f);
    motor.SetD(0, 0.0f);
    motor.SetF(0, 1.0f / 5700.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify
    std::cout << "CtrlType after set: " << static_cast<int>(motor.GetCtrlType()) << std::endl;
    std::cout << "kP after set: " << motor.GetP(0) << std::endl;

    run_phase(motor, "Phase 5: FeedbackSensor+CtrlType+SetSetpoint(500)", 5, [&]() {
      motor.SetSetpoint(500.0f);
    });

    // --- Stop ---
    std::cout << "\nStopping motor..." << std::endl;
    motor.SetCtrlType(CtrlType::kDutyCycle);
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
