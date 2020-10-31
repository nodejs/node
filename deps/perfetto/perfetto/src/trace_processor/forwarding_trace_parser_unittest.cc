/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/forwarding_trace_parser.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(TraceProcessorImplTest, GuessTraceType_Empty) {
  const uint8_t prefix[] = "";
  EXPECT_EQ(kUnknownTraceType, GuessTraceType(prefix, 0));
}

TEST(TraceProcessorImplTest, GuessTraceType_Json) {
  const uint8_t prefix[] = "{\"traceEvents\":[";
  EXPECT_EQ(kJsonTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_Ninja) {
  const uint8_t prefix[] = "# ninja log v5\n";
  EXPECT_EQ(kNinjaLogTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_JsonWithSpaces) {
  const uint8_t prefix[] = "\n{ \"traceEvents\": [";
  EXPECT_EQ(kJsonTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

// Some Android build traces do not contain the wrapper. See b/118826940
TEST(TraceProcessorImplTest, GuessTraceType_JsonMissingTraceEvents) {
  const uint8_t prefix[] = "[{";
  EXPECT_EQ(kJsonTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_Proto) {
  const uint8_t prefix[] = {0x0a, 0x00};  // An empty TracePacket.
  EXPECT_EQ(kProtoTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_Fuchsia) {
  const uint8_t prefix[] = {0x10, 0x00, 0x04, 0x46, 0x78, 0x54, 0x16, 0x00};
  EXPECT_EQ(kFuchsiaTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
