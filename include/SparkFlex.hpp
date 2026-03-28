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
 *
 * Note: Spark Flex uses the same API class 6 as Spark MAX for periodic status
 * frames and period-set commands. The voltage encoding in Period1 differs
 * (1 byte at 0.1V/count vs MAX's 2-byte 1/128V format) — handled by is_flex_
 * flag in SparkBase::ReadPeriodicMessages().
 */
class SparkFlex : public SparkBase
{
public:
  explicit SparkFlex(const std::string & interfaceName, uint8_t deviceId);
  ~SparkFlex() override = default;
};

#endif // SPARKFLEX_HPP
