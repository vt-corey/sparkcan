/**
 * @file SparkFlex.hpp
 * @brief Header file for the SparkFlex class for controlling REV Robotics SPARK Flex controllers
 * @author Grayson Arendt
 */

#ifndef SPARKFLEX_HPP
#define SPARKFLEX_HPP
#pragma once

#include "SparkBase.hpp"

/**
 * @class SparkFlex
 * @brief A class for controlling REV Robotics SPARK Flex via CAN bus
 *
 * This class provides methods to configure, control, and monitor the SPARK Flex.
 * It supports various control modes, parameter settings, and status readings.
 */
class SparkFlex : public SparkBase
{
public:
  explicit SparkFlex(const std::string & interfaceName, uint8_t deviceId);
  ~SparkFlex() override = default;

  /**
   * @brief Override: Spark Flex uses FlexPeriod1 (API class 46) instead of Period1 (class 6)
   */
  void SetPeriodicStatus1Period(uint16_t period) override;

  /**
   * @brief Override: Spark Flex uses FlexPeriod2 (API class 46) instead of Period2 (class 6)
   */
  void SetPeriodicStatus2Period(uint16_t period) override;

  /**
   * @brief Override: Spark Flex uses FlexPeriod3 (API class 46) instead of Period3 (class 6)
   */
  void SetPeriodicStatus3Period(uint16_t period) override;

  /**
   * @brief Override: Spark Flex uses FlexPeriod4 (API class 46) instead of Period4 (class 6)
   */
  void SetPeriodicStatus4Period(uint16_t period) override;
};

#endif // SPARKFLEX_HPP
