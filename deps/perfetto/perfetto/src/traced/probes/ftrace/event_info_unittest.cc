/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/traced/probes/ftrace/event_info.h"

#include "perfetto/protozero/proto_utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {
using protozero::proto_utils::ProtoSchemaType;

TEST(EventInfoTest, GetStaticEventInfoSanityCheck) {
  std::vector<Event> events = GetStaticEventInfo();
  for (const Event& event : events) {
    // For each event the following fields should be filled
    // statically:

    // Non-empty name, group, and proto field id.
    ASSERT_TRUE(event.name);
    ASSERT_TRUE(event.group);
    ASSERT_TRUE(event.proto_field_id);

    // Ftrace id and size should be zeroed.
    ASSERT_FALSE(event.ftrace_event_id);
    ASSERT_FALSE(event.size);

    for (const Field& field : event.fields) {
      // Non-empty name, proto field id, and proto field type.
      ASSERT_TRUE(field.ftrace_name);
      ASSERT_TRUE(field.proto_field_id);
      ASSERT_TRUE(static_cast<int>(field.proto_field_type));

      // Other fields should be zeroed.
      ASSERT_FALSE(field.ftrace_offset);
      ASSERT_FALSE(field.ftrace_size);
      ASSERT_FALSE(field.strategy);
      ASSERT_FALSE(field.ftrace_type);
    }
  }
}

TEST(EventInfoTest, GetStaticCommonFieldsInfoSanityCheck) {
  std::vector<Field> fields = GetStaticCommonFieldsInfo();
  for (const Field& field : fields) {
    // Non-empty name, group, and proto field id.
    ASSERT_TRUE(field.ftrace_name);
    ASSERT_TRUE(field.proto_field_id);
    ASSERT_TRUE(static_cast<int>(field.proto_field_type));

    // Other fields should be zeroed.
    ASSERT_FALSE(field.ftrace_offset);
    ASSERT_FALSE(field.ftrace_size);
    ASSERT_FALSE(field.strategy);
    ASSERT_FALSE(field.ftrace_type);
  }
}

TEST(EventInfoTest, SetTranslationStrategySanityCheck) {
  TranslationStrategy strategy = kUint32ToUint32;
  ASSERT_FALSE(SetTranslationStrategy(kFtraceCString, ProtoSchemaType::kUint64,
                                      &strategy));
  ASSERT_EQ(strategy, kUint32ToUint32);
  ASSERT_TRUE(SetTranslationStrategy(kFtraceCString, ProtoSchemaType::kString,
                                     &strategy));
  ASSERT_EQ(strategy, kCStringToString);
  ASSERT_TRUE(
      SetTranslationStrategy(kFtracePid32, ProtoSchemaType::kInt32, &strategy));
  ASSERT_EQ(strategy, kPid32ToInt32);
  ASSERT_TRUE(SetTranslationStrategy(kFtraceInode32, ProtoSchemaType::kUint64,
                                     &strategy));
  ASSERT_EQ(strategy, kInode32ToUint64);
  ASSERT_TRUE(SetTranslationStrategy(kFtraceInode64, ProtoSchemaType::kUint64,
                                     &strategy));
  ASSERT_EQ(strategy, kInode64ToUint64);
  ASSERT_TRUE(SetTranslationStrategy(kFtraceDevId32, ProtoSchemaType::kUint64,
                                     &strategy));
  ASSERT_EQ(strategy, kDevId32ToUint64);
  ASSERT_TRUE(SetTranslationStrategy(kFtraceDevId64, ProtoSchemaType::kUint64,
                                     &strategy));
  ASSERT_EQ(strategy, kDevId64ToUint64);
  ASSERT_TRUE(SetTranslationStrategy(kFtraceCommonPid32,
                                     ProtoSchemaType::kInt32, &strategy));
  ASSERT_EQ(strategy, kCommonPid32ToInt32);
}

}  // namespace
}  // namespace perfetto
