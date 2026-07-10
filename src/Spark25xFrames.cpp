// Spark 25.x frame encode/decode helpers — pure functions, no sockets.
// Layouts/scales: docs/spark-frames-2.0.0-dev.11.json (single source of truth).

#include "Spark25xFrames.hpp"

#include <cstring>

namespace spark25x
{

namespace
{
// FRC CAN arb id base for REV Spark: device type 2 (motor controller),
// manufacturer 5 (REV) -> (2 << 24) | (5 << 16) = 0x02050000.
constexpr uint32_t kArbBase = 0x02050000u;

// Assemble the 8-byte payload as a little-endian uint64 (same rawValue idiom
// as SparkBase::ReadPeriodicMessages) so bit positions from the JSON spec can
// be extracted with plain shifts.
uint64_t AssembleLE(const uint8_t * data)
{
    uint64_t raw = 0;
    for (int i = 0; i < 8; ++i)
    {
        raw |= static_cast<uint64_t>(data[i]) << (8 * i);
    }
    return raw;
}

float FloatAt(const uint8_t * data, size_t byteOffset)
{
    float f;
    std::memcpy(&f, data + byteOffset, sizeof(f));  // float32 LE per spec
    return f;
}
}  // namespace

uint32_t MakeArbId(uint8_t apiClass, uint8_t apiIndex, uint8_t deviceId)
{
    return kArbBase |
           (static_cast<uint32_t>(apiClass & 0x3F) << 10) |
           (static_cast<uint32_t>(apiIndex & 0x0F) << 6) |
           (deviceId & 0x3F);
}

// VELOCITY_SETPOINT / POSITION_SETPOINT (JSON spec): SETPOINT float32 @0,
// ARBITRARY_FEEDFORWARD int16 @32 (always 0 here), PID_SLOT 2 bits @48,
// ARBITRARY_FEEDFORWARD_UNITS 1 bit @50 (0 = voltage), rest reserved 0.
std::array<uint8_t, 8> EncodeSetpoint(float value, uint8_t pidSlot)
{
    std::array<uint8_t, 8> data{};
    std::memcpy(data.data(), &value, sizeof(value));  // float32 LE @0
    // bytes 4-5: arbitrary feedforward = 0 (already zeroed)
    data[6] = pidSlot & 0x03;  // PID slot bits 48-49
    return data;
}

// Heartbeat (SECONDARY_HEARTBEAT in the JSON spec): class 11 idx 2, sent to
// device 0; payload ENABLED_SPARKS_BITFIELD uint64 LE @0 (bit N enables devId N).
uint32_t HeartbeatArbId()
{
    return MakeArbId(11, 2, 0);
}

std::array<uint8_t, 8> EncodeHeartbeat(uint64_t deviceMask)
{
    std::array<uint8_t, 8> data{};
    for (int i = 0; i < 8; ++i)
    {
        data[i] = static_cast<uint8_t>(deviceMask >> (8 * i));
    }
    return data;
}

// PARAMETER_WRITE (JSON spec): class 14 idx 0, 5 bytes:
// PARAMETER_ID uint8 @0, VALUE uint32 LE @8.
ParamWriteFrame EncodeParamWrite(uint8_t deviceId, uint8_t paramId, uint32_t rawValue)
{
    ParamWriteFrame frame;
    frame.arbId = MakeArbId(14, 0, deviceId);
    frame.data[0] = paramId;
    frame.data[1] = static_cast<uint8_t>(rawValue);
    frame.data[2] = static_cast<uint8_t>(rawValue >> 8);
    frame.data[3] = static_cast<uint8_t>(rawValue >> 16);
    frame.data[4] = static_cast<uint8_t>(rawValue >> 24);
    return frame;
}

ParamWriteFrame EncodeParamWriteFloat(uint8_t deviceId, uint8_t paramId, float value)
{
    uint32_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    return EncodeParamWrite(deviceId, paramId, raw);
}

// PARAMETER_WRITE_RESPONSE (JSON spec): class 14 idx 1, 7 bytes:
// PARAMETER_ID uint8 @0, PARAMETER_TYPE uint8 @8, VALUE uint32 @16,
// RESULT_CODE uint8 @48 (0 = Success).
std::optional<ParamWriteResponse> DecodeParamWriteResponse(
    uint32_t arbId, const uint8_t * data, uint8_t dlc, uint8_t deviceId)
{
    if (arbId != MakeArbId(14, 1, deviceId) || dlc < 7)
    {
        return std::nullopt;
    }
    return ParamWriteResponse{data[0], data[6]};
}

// STATUS_0 (JSON spec): APPLIED_OUTPUT int16 @0 x 0.00003082369457075716,
// VOLTAGE uint12 @16 x 0.0073260073260073 V, CURRENT uint12 @28
// x 0.0366300366300366 A, MOTOR_TEMPERATURE uint8 @40 x 1 degC.
// Bits 48+ are limit/inverted/heartbeat-lock flags — NOT faults; not decoded here.
Status0 DecodeStatus0(const uint8_t * data)
{
    const uint64_t raw = AssembleLE(data);
    Status0 s;
    s.appliedOutput =
        static_cast<int16_t>(raw & 0xFFFF) * 0.00003082369457075716f;
    s.voltage = static_cast<float>((raw >> 16) & 0xFFF) * 0.0073260073260073f;
    s.current = static_cast<float>((raw >> 28) & 0xFFF) * 0.0366300366300366f;
    s.tempC = static_cast<float>((raw >> 40) & 0xFF);
    return s;
}

// STATUS_1 (JSON spec) is per-bit signals; packing used here:
//   faults       = active faults (bits 0-7: other, motor type, sensor, CAN,
//                  temperature, DRV, ESC EEPROM, firmware) in the LOW byte |
//                  active warnings (bits 16-23: brownout, overcurrent,
//                  ESC EEPROM, ext EEPROM, sensor, stall, has-reset, other)
//                  in the HIGH byte.
//   stickyFaults = sticky faults (bits 24-31, same order as faults) in the
//                  LOW byte | sticky warnings (bits 40-47, same order as
//                  warnings) in the HIGH byte.
Status1 DecodeStatus1(const uint8_t * data)
{
    const uint64_t raw = AssembleLE(data);
    Status1 s;
    const uint16_t faultBits = static_cast<uint16_t>(raw & 0xFF);           // @0-7
    const uint16_t warningBits = static_cast<uint16_t>((raw >> 16) & 0xFF); // @16-23
    const uint16_t stickyFaultBits = static_cast<uint16_t>((raw >> 24) & 0xFF);   // @24-31
    const uint16_t stickyWarningBits = static_cast<uint16_t>((raw >> 40) & 0xFF); // @40-47
    s.faults = static_cast<uint16_t>(faultBits | (warningBits << 8));
    s.stickyFaults = static_cast<uint16_t>(stickyFaultBits | (stickyWarningBits << 8));
    return s;
}

// STATUS_2 (JSON spec): PRIMARY_ENCODER_VELOCITY float32 @0 (RPM),
// PRIMARY_ENCODER_POSITION float32 @32 (rotations).
Status2 DecodeStatus2(const uint8_t * data)
{
    Status2 s;
    s.velocityRpm = FloatAt(data, 0);
    s.positionRot = FloatAt(data, 4);
    return s;
}

// STATUS_3 (JSON spec): ANALOG_POSITION float32 @32 (rotations, 0-1 per rev).
Status3 DecodeStatus3(const uint8_t * data)
{
    Status3 s;
    s.analogPosition = FloatAt(data, 4);
    return s;
}

// STATUS_5 (JSON spec): DUTY_CYCLE_ENCODER_VELOCITY float32 @0 (RPM),
// DUTY_CYCLE_ENCODER_POSITION float32 @32 (rotations, 0-1 per revolution).
// Bench-verified 2026-07-10 — community notes list velocity/position
// reversed; the JSON spec (and the bench) have velocity first.
Status5 DecodeStatus5(const uint8_t * data)
{
    Status5 s;
    s.velocityRpm = FloatAt(data, 0);
    s.positionRot = FloatAt(data, 4);
    return s;
}

uint32_t StatusArbId(uint8_t statusIdx, uint8_t deviceId)
{
    return MakeArbId(46, statusIdx, deviceId);
}

}  // namespace spark25x
