#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <functional>

#include "SparkFlex.hpp"

/*
 * Diagnostic test to determine if the Spark Flex is interpreting
 * the setpoint as velocity (RPM) or duty cycle (clamped to 1.0).
 *
 * Key phases:
 *   Phase A: DutyCycle(0.2) — known baseline ~20% speed
 *   Phase B: DutyCycle(1.0) — known full speed baseline
 *   Phase C: 3000 RPM with ctrl=1 — if velocity: ~53% speed; if duty clamped: full speed
 *   Phase D: 500 RPM with ctrl=1  — if velocity: ~9% speed;  if duty clamped: full speed
 *   Phase E: 100 RPM with ctrl=1  — if velocity: ~2% speed;  if duty clamped: full speed
 *
 * If C/D/E all run at the SAME speed as Phase B, it's clamping duty cycle.
 * If C is faster than D, and D is faster than E, it's doing velocity control.
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
  for (int i = 0; i < 15; i++) {
    motor.Heartbeat();
    motor.SetDutyCycle(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::cout << "  >> Observe speed. Press enter to continue..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main(int argc, char * argv[])
{
  uint8_t dev_id = 1;
  if (argc > 1) dev_id = std::atoi(argv[1]);

  std::cout << "Velocity vs DutyCycle diagnostic for device " << (int)dev_id << std::endl;
  std::cout << "Watch motor speed carefully in each phase.\n" << std::endl;

  try {
    SparkFlex motor("can0", dev_id);

    // Phase A: Known duty cycle baselines
    run_phase(motor, "Phase A: DutyCycle(0.2) — SLOW baseline", 4, [&]() {
      motor.SetDutyCycle(0.2f);
    });

    run_phase(motor, "Phase B: DutyCycle(1.0) — FULL SPEED baseline", 4, [&]() {
      motor.SetDutyCycle(1.0f);
    });

    // Phase C-E: Velocity setpoints via ctrl byte — if velocity control works,
    // these should run at DIFFERENT speeds proportional to the RPM value.
    // Using byte 6 for ctrl type.
    run_phase(motor, "Phase C: vel=3000 ctrl=1 @ byte6 — should be ~53% if velocity", 4, [&]() {
      motor.SendSetpointWithCtrlType(3000.0f, 1, 6);
    });

    run_phase(motor, "Phase D: vel=500 ctrl=1 @ byte6 — should be ~9% if velocity", 4, [&]() {
      motor.SendSetpointWithCtrlType(500.0f, 1, 6);
    });

    run_phase(motor, "Phase E: vel=100 ctrl=1 @ byte6 — should be ~2% if velocity", 4, [&]() {
      motor.SendSetpointWithCtrlType(100.0f, 1, 6);
    });

    // Phase F: Send 500.0 as plain DutyCycle (no ctrl byte) — if Flex clamps,
    // this should match Phase B (full speed)
    run_phase(motor, "Phase F: DutyCycle raw 500.0 (no ctrl byte) — clamping test", 4, [&]() {
      motor.SendSetpointWithCtrlType(500.0f, 0, 6);  // ctrl=0 means kDutyCycle
    });

    std::cout << "\nAll phases complete." << std::endl;
    std::cout << "\nKey question: Did C > D > E in speed? If yes, velocity control works!" << std::endl;
    std::cout << "Did C/D/E all match Phase B? If yes, it's just clamping duty cycle." << std::endl;

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
