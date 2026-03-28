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
 * frames and period-set commands. Period1 frame format (velocity, temperature,
 * voltage, current) is identical between Flex and MAX on class 6.
 */
class SparkFlex : public SparkBase
{
public:
  explicit SparkFlex(const std::string & interfaceName, uint8_t deviceId);
  ~SparkFlex() override = default;
};

#endif // SPARKFLEX_HPP
