#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "Spark25xFrames.hpp"
using namespace spark25x;

TEST(ArbIds, StatusAndControl) {
  EXPECT_EQ(StatusArbId(0, 1), 0x0205B801u);
  EXPECT_EQ(StatusArbId(2, 8), 0x0205B888u);
  EXPECT_EQ(MakeArbId(kVelocityApiClass, kVelocityApiIndex, 1), 0x02050001u);
  EXPECT_EQ(MakeArbId(kPositionApiClass, kPositionApiIndex, 2), 0x02050102u);
  EXPECT_EQ(HeartbeatArbId(), 0x02052C80u);
}

TEST(Encode, VelocitySetpoint300Slot1) {
  auto d = EncodeSetpoint(300.0f, 1);
  const uint8_t expect[8] = {0x00, 0x00, 0x96, 0x43, 0x00, 0x00, 0x01, 0x00};
  EXPECT_TRUE(std::equal(d.begin(), d.end(), expect));  // bench-verified frame
}

TEST(Encode, HeartbeatAllEnable) {
  auto d = EncodeHeartbeat(0xFFFFFFFFFFFFFFFFull);
  for (auto b : d) EXPECT_EQ(b, 0xFF);
}

TEST(Encode, ParamWriteForceEnableStatus2) {
  auto f = EncodeParamWrite(1, kForceEnableStatus2, 1);
  EXPECT_EQ(f.arbId, 0x02053801u);
  const uint8_t expect[5] = {0xBC, 0x01, 0x00, 0x00, 0x00};  // bench-verified
  EXPECT_TRUE(std::equal(f.data.begin(), f.data.end(), expect));
}

TEST(Decode, Status0RealFrame) {
  // captured 2026-07-09: idle, 24C, ~12.8V (spec scale 0.0073260073)
  const uint8_t d[8] = {0x00, 0x00, 0xD1, 0x06, 0x00, 0x18, 0x80, 0x00};
  auto s = DecodeStatus0(d);
  EXPECT_FLOAT_EQ(s.appliedOutput, 0.0f);
  EXPECT_NEAR(s.voltage, 0x6D1 * 0.0073260073f, 1e-3);
  EXPECT_FLOAT_EQ(s.current, 0.0f);
  EXPECT_FLOAT_EQ(s.tempC, 24.0f);
}

TEST(Decode, Status2RealFrame) {
  // captured mid-spin: 62.86 RPM, 4.417 rot
  const uint8_t d[8] = {0x01, 0x6F, 0x7B, 0x42, 0x3F, 0x55, 0x8D, 0x40};
  auto s = DecodeStatus2(d);
  EXPECT_NEAR(s.velocityRpm, 62.86f, 0.01f);
  EXPECT_NEAR(s.positionRot, 4.4166f, 0.001f);
}

TEST(Decode, ParamWriteResponseMatchesDevice) {
  // response arb = class 14 idx 1 = 0x02053840|dev; layout per JSON spec
  // (PARAMETER_WRITE_RESPONSE, lengthBytes 7): PARAMETER_ID uint8 @0,
  // PARAMETER_TYPE uint8 @8 (2 = Uint), VALUE uint32 LE @16,
  // RESULT_CODE uint8 @48 (0 = Success).
  // Success response for paramId 188 (kForceEnableStatus2), value 1, device 3.
  const uint8_t d[7] = {0xBC, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00};
  auto r = DecodeParamWriteResponse(0x02053843u, d, 7, 3);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->paramId, 188);
  EXPECT_EQ(r->resultCode, 0);
  // Same frame, wrong device id — not ours.
  EXPECT_FALSE(DecodeParamWriteResponse(0x02053843u, d, 7, 4).has_value());
  // Wrong arb id (Status 0 frame for the same device) — not a param response.
  EXPECT_FALSE(DecodeParamWriteResponse(0x0205B803u, d, 7, 3).has_value());
}

TEST(Decode, ParamWriteResponseShortDlcRejected) {
  // A truncated response (dlc < 7 per PARAMETER_WRITE_RESPONSE lengthBytes 7)
  // must be rejected even when the arb id matches — the result code lives in
  // byte 6, which a short frame does not carry.
  const uint8_t d[7] = {0xBC, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00};
  EXPECT_FALSE(DecodeParamWriteResponse(0x02053843u, d, 6, 3).has_value());
  EXPECT_FALSE(DecodeParamWriteResponse(0x02053843u, d, 0, 3).has_value());
}

TEST(Decode, Status1SyntheticBitPacking) {
  // One bit set in each spec range: fault bit 2 (SENSOR_FAULT), warning
  // bit 18 (ESC_EEPROM_WARNING), sticky fault bit 27, sticky warning bit 44.
  // Packing per DecodeStatus1: faults = fault byte | warning byte << 8;
  // stickyFaults = sticky fault byte | sticky warning byte << 8.
  uint8_t d[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  d[0] = 1u << 2;         // bit 2
  d[2] = 1u << (18 - 16); // bit 18
  d[3] = 1u << (27 - 24); // bit 27
  d[5] = 1u << (44 - 40); // bit 44
  auto s = DecodeStatus1(d);
  EXPECT_EQ(s.faults, (1u << 2) | (1u << 10));
  EXPECT_EQ(s.stickyFaults, (1u << 3) | (1u << 12));
}

TEST(Decode, Status3FloatAtByte4) {
  // ANALOG_POSITION is float32 @ bit 32 (byte 4). Bytes 0-3 are nonzero
  // garbage to prove the decoder reads at the right offset.
  const uint8_t d[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x80, 0x3E};
  auto s = DecodeStatus3(d);
  EXPECT_FLOAT_EQ(s.analogPosition, 0.25f);
}

TEST(Decode, Status5PositionAtByte4) {
  // STATUS_5 (duty-cycle absolute encoder): DUTY_CYCLE_ENCODER_VELOCITY
  // float32 @ bit 0, DUTY_CYCLE_ENCODER_POSITION float32 @ bit 32 (byte 4).
  // Community notes list these reversed — the JSON spec and the bench agree
  // on velocity-first. Bytes 0-3 are nonzero garbage to prove the position
  // decode reads at byte 4, not byte 0.
  const uint8_t d[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x80, 0x3E};
  auto s = DecodeStatus5(d);
  EXPECT_FLOAT_EQ(s.positionRot, 0.25f);
  // And the velocity really comes from bytes 0-3 (the "garbage" float).
  float garbage;
  std::memcpy(&garbage, d, sizeof(garbage));
  EXPECT_FLOAT_EQ(s.velocityRpm, garbage);
}

TEST(Encode, HeartbeatSingleDeviceBit) {
  // Bit 8 of the LE uint64 bitfield lands in byte 1 only.
  auto d = EncodeHeartbeat(1ull << 8);
  for (size_t i = 0; i < d.size(); ++i) {
    EXPECT_EQ(d[i], i == 1 ? 0x01 : 0x00) << "byte " << i;
  }
}

TEST(Encode, ParamWriteFloatRoundTrip) {
  // Float values travel as raw IEEE 754 bits in bytes 1-4 — reinterpreting
  // the payload must give back the exact float.
  auto f = EncodeParamWriteFloat(1, kP0, 0.0002f);
  EXPECT_EQ(f.data[0], kP0);
  float roundTripped;
  std::memcpy(&roundTripped, f.data.data() + 1, sizeof(roundTripped));
  EXPECT_EQ(roundTripped, 0.0002f);
}

TEST(Encode, SetpointSlotMasked) {
  // PID slot is a 2-bit field (bits 48-49): slot 5 (0b101) must be masked
  // to 0b01, leaving bits 50+ untouched.
  auto d = EncodeSetpoint(0.0f, 5);
  EXPECT_EQ(d[6], 0x01);
  EXPECT_EQ(d[7], 0x00);
}

TEST(Encode, SetEncoderPositionZero) {
  auto f = EncodeSetEncoderPosition(1, 0.0f);
  EXPECT_EQ(f.arbId, 0x02052801u);
  const uint8_t expect[5] = {0x00, 0x00, 0x00, 0x00, 0x03};
  EXPECT_TRUE(std::equal(f.data.begin(), f.data.end(), expect));
}

TEST(Decode, Status4Shape) {
  const uint8_t d[8] = {0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F};  // vel=1.0, pos=0.5
  auto s = DecodeStatus4(d);
  EXPECT_FLOAT_EQ(s.velocity, 1.0f);
  EXPECT_FLOAT_EQ(s.position, 0.5f);
}
