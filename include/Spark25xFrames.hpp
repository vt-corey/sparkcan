#ifndef SPARK25X_FRAMES_HPP
#define SPARK25X_FRAMES_HPP
#include <array>
#include <cstdint>
#include <optional>

namespace spark25x
{
// Arb id helper: devType 2, mfr 5. apiClass 6 bits, apiIndex 4 bits, devId 6 bits.
uint32_t MakeArbId(uint8_t apiClass, uint8_t apiIndex, uint8_t deviceId);

// Setpoint payloads (8 bytes): float32 LE value @0, int16 LE arbFF=0 @32,
// PID slot (2 bits) @48. Spec: VELOCITY_SETPOINT / POSITION_SETPOINT.
std::array<uint8_t, 8> EncodeSetpoint(float value, uint8_t pidSlot);
// Velocity: class 0 idx 0. Position: class 0 idx 4. Duty: class 0 idx 2 (payload
// is float only — reuse EncodeSetpoint with slot 0; firmware ignores slot for duty).
constexpr uint8_t kVelocityApiClass = 0, kVelocityApiIndex = 0;
constexpr uint8_t kPositionApiClass = 0, kPositionApiIndex = 4;

// Heartbeat: class 11 idx 2, device 0, 64-bit enable bitfield LE.
uint32_t HeartbeatArbId();
std::array<uint8_t, 8> EncodeHeartbeat(uint64_t deviceMask);

// Parameter write: class 14 idx 0, 5 bytes: id @0, value LE @1..4.
struct ParamWriteFrame { uint32_t arbId; std::array<uint8_t, 5> data; };
ParamWriteFrame EncodeParamWrite(uint8_t deviceId, uint8_t paramId, uint32_t rawValue);
ParamWriteFrame EncodeParamWriteFloat(uint8_t deviceId, uint8_t paramId, float value);
// Response: class 14 idx 1. Returns {paramId, resultCode} if the frame is a
// param-write response for this device, else nullopt. Result code bit position
// FROM THE JSON SPEC (PARAMETER_WRITE_RESPONSE).
struct ParamWriteResponse { uint8_t paramId; uint8_t resultCode; };
std::optional<ParamWriteResponse> DecodeParamWriteResponse(
  uint32_t arbId, const uint8_t * data, uint8_t dlc, uint8_t deviceId);

// Status decode. Layouts/scales from docs/spark-frames-2.0.0-dev.11.json.
struct Status0 { float appliedOutput; float voltage; float current; float tempC; };
struct Status1 { uint16_t faults; uint16_t stickyFaults; };  // bit ranges per spec
struct Status2 { float velocityRpm; float positionRot; };
struct Status3 { float analogPosition; };  // float @32, 0-1 per rev
// Status 5: duty-cycle absolute encoder (data port). VELOCITY float32 @0,
// POSITION float32 @32 (0-1 per revolution) — bench-verified 2026-07-10;
// community notes list these reversed.
struct Status5 { float velocityRpm; float positionRot; };
Status0 DecodeStatus0(const uint8_t * data);
Status1 DecodeStatus1(const uint8_t * data);
Status2 DecodeStatus2(const uint8_t * data);
Status3 DecodeStatus3(const uint8_t * data);
Status5 DecodeStatus5(const uint8_t * data);

// Class-46 status arb ids: class 46, idx = status number.
uint32_t StatusArbId(uint8_t statusIdx, uint8_t deviceId);

// Key parameter ids (from docs/spark-max-25x-protocol.md table)
enum Param25x : uint8_t {
  kP0 = 13, kI0 = 14, kD0 = 15, kF0 = 16, kOutMin0 = 19, kOutMax0 = 20,
  kP1 = 21, kI1 = 22, kD1 = 23, kF1 = 24, kOutMin1 = 27, kOutMax1 = 28,
  kPositionConversionFactor = 112, kVelocityConversionFactor = 113,
  kStatusPeriod2 = 160, kStatusPeriod3 = 161, kStatusPeriod5 = 163,
  kForceEnableStatus2 = 188, kForceEnableStatus3 = 189, kForceEnableStatus5 = 191,
};
}  // namespace spark25x
#endif
