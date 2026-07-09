#include <gtest/gtest.h>

#include <algorithm>

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
