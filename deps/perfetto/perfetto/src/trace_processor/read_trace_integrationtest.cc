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

#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/trace_processor/read_trace.h"

#include "src/base/test/utils.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(ReadTraceIntegrationTest, CompressedTrace) {
  base::ScopedFstream f(fopen(
      base::GetTestDataPath(std::string("test/data/compressed.pb")).c_str(),
      "rb"));
  std::vector<uint8_t> raw_trace;
  while (!feof(*f)) {
    uint8_t buf[4096];
    auto rsize =
        fread(reinterpret_cast<char*>(buf), 1, base::ArraySize(buf), *f);
    raw_trace.insert(raw_trace.end(), buf, buf + rsize);
  }

  std::vector<uint8_t> decompressed;
  decompressed.reserve(raw_trace.size());

  util::Status status = trace_processor::DecompressTrace(
      raw_trace.data(), raw_trace.size(), &decompressed);
  ASSERT_TRUE(status.ok());

  protos::pbzero::Trace::Decoder decoder(decompressed.data(),
                                         decompressed.size());
  uint32_t packet_count = 0;
  for (auto it = decoder.packet(); it; ++it) {
    protos::pbzero::TracePacket::Decoder packet(*it);
    ASSERT_FALSE(packet.has_compressed_packets());
    ++packet_count;
  }
  ASSERT_EQ(packet_count, 2412u);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
