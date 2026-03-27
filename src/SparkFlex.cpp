/**
 * @file SparkFlex.cpp
 * @brief Source file for the SparkFlex class for controlling REV Robotics SPARK Flex controllers
 * @author Grayson Arendt
 */

#include "SparkFlex.hpp"

SparkFlex::SparkFlex(const std::string & interfaceName, uint8_t deviceId)
: SparkBase(interfaceName, deviceId) {}

// Spark Flex uses API class 46 for periodic status frames 1-4,
// not class 6 (which is Spark MAX format and is silently ignored by Flex firmware).

void SparkFlex::SetPeriodicStatus1Period(uint16_t period)
{
  std::vector<uint8_t> data(2, 0x00);
  data[0] = static_cast<uint8_t>(period & 0xFF);
  data[1] = static_cast<uint8_t>((period >> 8) & 0xFF);
  SendCanFrame(APICommand::FlexPeriod1, data);
}

void SparkFlex::SetPeriodicStatus2Period(uint16_t period)
{
  std::vector<uint8_t> data(2, 0x00);
  data[0] = static_cast<uint8_t>(period & 0xFF);
  data[1] = static_cast<uint8_t>((period >> 8) & 0xFF);
  SendCanFrame(APICommand::FlexPeriod2, data);
}

void SparkFlex::SetPeriodicStatus3Period(uint16_t period)
{
  std::vector<uint8_t> data(2, 0x00);
  data[0] = static_cast<uint8_t>(period & 0xFF);
  data[1] = static_cast<uint8_t>((period >> 8) & 0xFF);
  SendCanFrame(APICommand::FlexPeriod3, data);
}

void SparkFlex::SetPeriodicStatus4Period(uint16_t period)
{
  std::vector<uint8_t> data(2, 0x00);
  data[0] = static_cast<uint8_t>(period & 0xFF);
  data[1] = static_cast<uint8_t>((period >> 8) & 0xFF);
  SendCanFrame(APICommand::FlexPeriod4, data);
}
