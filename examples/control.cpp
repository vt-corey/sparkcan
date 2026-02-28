#include <iostream>

#include "SparkFlex.hpp"

/*
This has been tested with the SPARK MAX while connected to an AndyMark 775
RedLine Motor and with a Spark Flex connected to a NEO Vortex Brushless Motor.
*/

int main() {
  try {
    // Initialize SparkFlex object with CAN interface and CAN ID
    SparkFlex motor("can0", 1);

    // Configure and burn parameters for NEO Vortex
    motor.SetIdleMode(IdleMode::kBrake);
    motor.SetMotorType(MotorType::kBrushless);
    motor.SetSensorType(SensorType::kHallSensor);
    motor.SetRampRate(0.1);
    motor.SetInverted(false);
    motor.SetMotorKv(565);
    motor.SetEncoderCountsPerRev(7168);
    motor.SetSmartCurrentFreeLimit(20.0);
    motor.SetSmartCurrentStallLimit(20.0);
    motor.BurnFlash();

    // Loop for 10 seconds
    auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::high_resolution_clock::now() - start)
               .count() < 30) {
      // Enable and run motor
      motor.Heartbeat();
      motor.SetDutyCycle(-0.2);
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}
