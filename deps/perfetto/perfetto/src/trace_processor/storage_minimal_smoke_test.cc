/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <cstdio>
#include <string>

#include <json/reader.h>
#include <json/value.h>

#include "perfetto/ext/trace_processor/export_json.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/trace_processor_storage.h"
#include "src/base/test/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

class JsonStringOutputWriter : public json::OutputWriter {
 public:
  util::Status AppendString(const std::string& string) override {
    buffer += string;
    return util::OkStatus();
  }
  std::string buffer;
};

class StorageMinimalSmokeTest : public ::testing::Test {
 public:
  StorageMinimalSmokeTest()
      : storage_(TraceProcessorStorage::CreateInstance(Config())) {}

 protected:
  std::unique_ptr<TraceProcessorStorage> storage_;
};

TEST_F(StorageMinimalSmokeTest, GraphicEventsIgnored) {
  const size_t MAX_SIZE = 1 << 20;
  auto f = fopen(base::GetTestDataPath("test/data/gpu_trace.pb").c_str(), "rb");
  std::unique_ptr<uint8_t[]> buf(new uint8_t[MAX_SIZE]);
  auto rsize = fread(reinterpret_cast<char*>(buf.get()), 1, MAX_SIZE, f);
  util::Status status = storage_->Parse(std::move(buf), rsize);
  ASSERT_TRUE(status.ok());
  storage_->NotifyEndOfFile();

  JsonStringOutputWriter output_writer;
  json::ExportJson(storage_.get(), &output_writer);
  Json::Reader reader;
  Json::Value result;
  reader.parse(output_writer.buffer, result);

  ASSERT_EQ(result["traceEvents"].size(), 0u);
}

TEST_F(StorageMinimalSmokeTest, SystraceReturnsError) {
  const size_t MAX_SIZE = 1 << 20;
  auto f =
      fopen(base::GetTestDataPath("test/data/systrace.html").c_str(), "rb");
  std::unique_ptr<uint8_t[]> buf(new uint8_t[MAX_SIZE]);
  auto rsize = fread(reinterpret_cast<char*>(buf.get()), 1, MAX_SIZE, f);
  util::Status status = storage_->Parse(std::move(buf), rsize);

  ASSERT_FALSE(status.ok());
}

TEST_F(StorageMinimalSmokeTest, TrackEventsImported) {
  const size_t MAX_SIZE = 1 << 20;
  auto f = fopen("test/trace_processor/track_event_typed_args.pb", "rb");
  std::unique_ptr<uint8_t[]> buf(new uint8_t[MAX_SIZE]);
  auto rsize = fread(reinterpret_cast<char*>(buf.get()), 1, MAX_SIZE, f);
  util::Status status = storage_->Parse(std::move(buf), rsize);
  ASSERT_TRUE(status.ok());
  storage_->NotifyEndOfFile();

  JsonStringOutputWriter output_writer;
  json::ExportJson(storage_.get(), &output_writer);
  Json::Reader reader;
  Json::Value result;
  reader.parse(output_writer.buffer, result);

  ASSERT_EQ(result["traceEvents"].size(), 4u);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
