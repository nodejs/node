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

#include "src/traced/probes/ftrace/cpu_reader.h"

#include <string.h>
#include <sys/stat.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/protozero/scattered_stream_writer.h"
#include "src/traced/probes/ftrace/event_info.h"
#include "src/traced/probes/ftrace/ftrace_config_muxer.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"
#include "src/traced/probes/ftrace/test/cpu_reader_support.h"
#include "src/tracing/core/trace_writer_for_testing.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/ftrace/ftrace.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "protos/perfetto/trace/ftrace/sched.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "src/traced/probes/ftrace/test/test_messages.gen.h"
#include "src/traced/probes/ftrace/test/test_messages.pbzero.h"

using protozero::proto_utils::ProtoSchemaType;
using testing::_;
using testing::AnyNumber;
using testing::Contains;
using testing::Each;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::EndsWith;
using testing::Eq;
using testing::NiceMock;
using testing::Pair;
using testing::Return;
using testing::StartsWith;

namespace perfetto {

namespace {

FtraceDataSourceConfig EmptyConfig() {
  return FtraceDataSourceConfig{
      EventFilter{}, DisabledCompactSchedConfigForTesting(), {}, {}};
}

constexpr uint64_t kNanoInSecond = 1000 * 1000 * 1000;
constexpr uint64_t kNanoInMicro = 1000;

::testing::AssertionResult WithinOneMicrosecond(uint64_t actual_ns,
                                                uint64_t expected_s,
                                                uint64_t expected_us) {
  // Round to closest us.
  uint64_t actual_us = (actual_ns + kNanoInMicro / 2) / kNanoInMicro;
  uint64_t total_expected_us = expected_s * 1000 * 1000 + expected_us;
  if (actual_us == total_expected_us)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << actual_ns / kNanoInSecond << "."
         << (actual_ns % kNanoInSecond) / kNanoInMicro << " vs. " << expected_s
         << "." << expected_us;
}

class MockFtraceProcfs : public FtraceProcfs {
 public:
  MockFtraceProcfs() : FtraceProcfs("/root/") {
    ON_CALL(*this, NumberOfCpus()).WillByDefault(Return(1));
    ON_CALL(*this, WriteToFile(_, _)).WillByDefault(Return(true));
    ON_CALL(*this, ClearFile(_)).WillByDefault(Return(true));
    EXPECT_CALL(*this, NumberOfCpus()).Times(AnyNumber());
  }

  MOCK_METHOD2(WriteToFile,
               bool(const std::string& path, const std::string& str));
  MOCK_METHOD1(ReadOneCharFromFile, char(const std::string& path));
  MOCK_METHOD1(ClearFile, bool(const std::string& path));
  MOCK_CONST_METHOD1(ReadFileIntoString, std::string(const std::string& path));
  MOCK_CONST_METHOD0(NumberOfCpus, size_t());
};

class CpuReaderTableTest : public ::testing::Test {
 protected:
  NiceMock<MockFtraceProcfs> ftrace_;
};

// Single class to manage the whole protozero -> scattered stream -> chunks ->
// single buffer -> real proto dance. Has a method: writer() to get an
// protozero ftrace bundle writer and a method ParseProto() to attempt to
// parse whatever has been written so far into a proto message.
template <class ZeroT, class ProtoT>
class ProtoProvider {
 public:
  explicit ProtoProvider(size_t chunk_size)
      : chunk_size_(chunk_size), delegate_(chunk_size_), stream_(&delegate_) {
    delegate_.set_writer(&stream_);
    writer_.Reset(&stream_);
  }
  ~ProtoProvider() = default;

  ZeroT* writer() { return &writer_; }

  // Stitch together the scattered chunks into a single buffer then attempt
  // to parse the buffer as a FtraceEventBundle. Returns the FtraceEventBundle
  // on success and nullptr on failure.
  std::unique_ptr<ProtoT> ParseProto() {
    auto bundle = std::unique_ptr<ProtoT>(new ProtoT());
    std::vector<uint8_t> buffer = delegate_.StitchSlices();
    if (!bundle->ParseFromArray(buffer.data(), buffer.size())) {
      return nullptr;
    }
    return bundle;
  }

 private:
  ProtoProvider(const ProtoProvider&) = delete;
  ProtoProvider& operator=(const ProtoProvider&) = delete;

  size_t chunk_size_;
  protozero::ScatteredHeapBuffer delegate_;
  protozero::ScatteredStreamWriter stream_;
  ZeroT writer_;
};

using BundleProvider = ProtoProvider<protos::pbzero::FtraceEventBundle,
                                     protos::gen::FtraceEventBundle>;

class BinaryWriter {
 public:
  BinaryWriter()
      : size_(base::kPageSize), page_(new uint8_t[size_]), ptr_(page_.get()) {}

  template <typename T>
  void Write(T t) {
    memcpy(ptr_, &t, sizeof(T));
    ptr_ += sizeof(T);
    PERFETTO_CHECK(ptr_ < ptr_ + size_);
  }

  void WriteFixedString(size_t n, const char* s) {
    size_t length = strlen(s);
    PERFETTO_CHECK(length < n);
    char c;
    while ((c = *s++)) {
      Write<char>(c);
    }
    Write<char>('\0');
    for (size_t i = 0; i < n - length - 1; i++) {
      Write<char>('\xff');
    }
  }

  std::unique_ptr<uint8_t[]> GetCopy() {
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[written()]);
    memcpy(buffer.get(), page_.get(), written());
    return buffer;
  }

  size_t written() { return static_cast<size_t>(ptr_ - page_.get()); }

 private:
  size_t size_;
  std::unique_ptr<uint8_t[]> page_;
  uint8_t* ptr_;
};

}  // namespace

TEST(PageFromXxdTest, OneLine) {
  std::string text = R"(
    00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................
    00000000: 0000 0000 5600 0000 0000 0000 0000 0000  ................
  )";
  auto page = PageFromXxd(text);
  EXPECT_EQ(page.get()[0x14], 0x56);
}

TEST(PageFromXxdTest, ManyLines) {
  std::string text = R"(
    00000000: 1234 0000 0000 0000 0000 0000 0000 0056  ................
    00000010: 7800 0000 0000 0000 0000 0000 0000 009a  ................
    00000020: 0000 0000 bc00 0000 00de 0000 0000 009a  ................
  )";
  auto page = PageFromXxd(text);
  EXPECT_EQ(page.get()[0x00], 0x12);
  EXPECT_EQ(page.get()[0x01], 0x34);
  EXPECT_EQ(page.get()[0x0f], 0x56);
  EXPECT_EQ(page.get()[0x10], 0x78);
  EXPECT_EQ(page.get()[0x1f], 0x9a);
  EXPECT_EQ(page.get()[0x24], 0xbc);
  EXPECT_EQ(page.get()[0x29], 0xde);
}

TEST(CpuReaderTest, BinaryWriter) {
  BinaryWriter writer;
  writer.Write<uint64_t>(1);
  writer.Write<uint32_t>(2);
  writer.Write<uint16_t>(3);
  writer.Write<uint8_t>(4);
  auto buffer = writer.GetCopy();
  EXPECT_EQ(buffer.get()[0], 1);
  EXPECT_EQ(buffer.get()[1], 0);
  EXPECT_EQ(buffer.get()[2], 0);
  EXPECT_EQ(buffer.get()[3], 0);
  EXPECT_EQ(buffer.get()[4], 0);
  EXPECT_EQ(buffer.get()[5], 0);
  EXPECT_EQ(buffer.get()[6], 0);
  EXPECT_EQ(buffer.get()[7], 0);
  EXPECT_EQ(buffer.get()[8], 2);
}

TEST(ReadAndAdvanceTest, Number) {
  uint64_t expected = 42;
  uint64_t actual = 0;
  uint8_t buffer[8] = {};
  const uint8_t* start = buffer;
  const uint8_t* ptr = buffer;
  memcpy(&buffer, &expected, 8);
  EXPECT_TRUE(CpuReader::ReadAndAdvance<uint64_t>(&ptr, ptr + 8, &actual));
  EXPECT_EQ(ptr, start + 8);
  EXPECT_EQ(actual, expected);
}

TEST(ReadAndAdvanceTest, PlainStruct) {
  struct PlainStruct {
    uint64_t timestamp;
    uint64_t length;
  };

  uint64_t expected[2] = {42, 999};
  PlainStruct actual;
  uint8_t buffer[16] = {};
  const uint8_t* start = buffer;
  const uint8_t* ptr = buffer;
  memcpy(&buffer, &expected, 16);
  EXPECT_TRUE(CpuReader::ReadAndAdvance<PlainStruct>(&ptr, ptr + 16, &actual));
  EXPECT_EQ(ptr, start + 16);
  EXPECT_EQ(actual.timestamp, 42ul);
  EXPECT_EQ(actual.length, 999ul);
}

TEST(ReadAndAdvanceTest, ComplexStruct) {
  struct ComplexStruct {
    uint64_t timestamp;
    uint32_t length;
    uint32_t : 24;
    uint32_t overwrite : 8;
  };

  uint64_t expected[2] = {42, 0xcdffffffabababab};
  ComplexStruct actual = {};
  uint8_t buffer[16] = {};
  const uint8_t* start = buffer;
  const uint8_t* ptr = buffer;
  memcpy(&buffer, &expected, 16);
  EXPECT_TRUE(
      CpuReader::ReadAndAdvance<ComplexStruct>(&ptr, ptr + 16, &actual));
  EXPECT_EQ(ptr, start + 16);
  EXPECT_EQ(actual.timestamp, 42ul);
  EXPECT_EQ(actual.length, 0xabababab);
  EXPECT_EQ(actual.overwrite, 0xCDu);
}

TEST(ReadAndAdvanceTest, Overruns) {
  uint64_t result = 42;
  uint8_t buffer[7] = {};
  const uint8_t* start = buffer;
  const uint8_t* ptr = buffer;
  EXPECT_FALSE(CpuReader::ReadAndAdvance<uint64_t>(&ptr, ptr + 7, &result));
  EXPECT_EQ(ptr, start);
  EXPECT_EQ(result, 42ul);
}

TEST(ReadAndAdvanceTest, AtEnd) {
  uint8_t result = 42;
  uint8_t buffer[8] = {};
  const uint8_t* start = buffer;
  const uint8_t* ptr = buffer;
  EXPECT_FALSE(CpuReader::ReadAndAdvance<uint8_t>(&ptr, ptr, &result));
  EXPECT_EQ(ptr, start);
  EXPECT_EQ(result, 42);
}

TEST(ReadAndAdvanceTest, Underruns) {
  uint64_t expected = 42;
  uint64_t actual = 0;
  uint8_t buffer[9] = {};
  const uint8_t* start = buffer;
  const uint8_t* ptr = buffer;
  memcpy(&buffer, &expected, 8);
  EXPECT_TRUE(CpuReader::ReadAndAdvance<uint64_t>(&ptr, ptr + 8, &actual));
  EXPECT_EQ(ptr, start + 8);
  EXPECT_EQ(actual, expected);
}

TEST(ParsePageHeaderTest, WithOverrun) {
  std::string text = R"(
    00000000: 3ef3 db77 67a2 0100 f00f 0080 ffff ffff
    )";
  auto page = PageFromXxd(text);

  // parse as if we're on a 32 bit kernel (4 byte "commit" field)
  {
    const uint8_t* ptr = page.get();
    auto ret = CpuReader::ParsePageHeader(&ptr, 4u);
    ASSERT_TRUE(ret.has_value());
    CpuReader::PageHeader parsed = ret.value();

    ASSERT_EQ(parsed.timestamp, 0x0001A26777DBF33Eull);  // first 8 bytes
    ASSERT_EQ(parsed.size, 0x0ff0u);                     // 4080
    ASSERT_TRUE(parsed.lost_events);

    // pointer advanced past the header (8+4 bytes)
    ASSERT_EQ(ptr, page.get() + 12);
  }

  // parse as if we're on a 64 bit kernel (8 byte "commit" field)
  {
    const uint8_t* ptr = page.get();
    auto ret = CpuReader::ParsePageHeader(&ptr, 8u);
    ASSERT_TRUE(ret.has_value());
    CpuReader::PageHeader parsed = ret.value();

    ASSERT_EQ(parsed.timestamp, 0x0001A26777DBF33Eull);  // first 8 bytes
    ASSERT_EQ(parsed.size, 0x0ff0u);                     // 4080
    ASSERT_TRUE(parsed.lost_events);

    // pointer advanced past the header (8+8 bytes)
    ASSERT_EQ(ptr, page.get() + 16);
  }
}

// clang-format off
// # tracer: nop
// #
// # entries-in-buffer/entries-written: 1/1   #P:8
// #
// #                              _-----=> irqs-off
// #                             / _----=> need-resched
// #                            | / _---=> hardirq/softirq
// #                            || / _--=> preempt-depth
// #                            ||| /     delay
// #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
// #              | |       |   ||||       |         |
//               sh-28712 [000] ...1 608934.535199: tracing_mark_write: Hello, world!
// clang-format on

static ExamplePage g_single_print{
    "synthetic",
    R"(
    00000000: ba12 6a33 c628 0200 2c00 0000 0000 0000  ..j3.(..,.......
    00000010: def0 ec67 8d21 0000 0800 0000 0500 0001  ...g.!..........
    00000020: 2870 0000 ac5d 1661 86ff ffff 4865 6c6c  (p...].a....Hell
    00000030: 6f2c 2077 6f72 6c64 210a 00ff 0000 0000  o, world!.......
  )",
};

TEST(CpuReaderTest, ParseSinglePrint) {
  const ExamplePage* test_case = &g_single_print;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("ftrace", "print")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_EQ(44ul, page_header->size);
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_EQ(evt_bytes, 44ul);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  ASSERT_EQ(bundle->event().size(), 1u);
  const protos::gen::FtraceEvent& event = bundle->event()[0];
  EXPECT_EQ(event.pid(), 28712ul);
  EXPECT_TRUE(WithinOneMicrosecond(event.timestamp(), 608934, 535199));
  EXPECT_EQ(event.print().buf(), "Hello, world!\n");
}

// clang-format off
// # tracer: nop
// #
// # entries-in-buffer/entries-written: 2/2   #P:8
// #
// #                                      _-----=> irqs-off
// #                                     / _----=> need-resched
// #                                    | / _---=> hardirq/softirq
// #                                    || / _--=> preempt-depth
// #                                    ||| /     delay
// #           TASK-PID    TGID   CPU#  ||||    TIMESTAMP  FUNCTION
// #              | |        |      |   ||||       |         |
//             echo-6908  ( 6908) [000] ...1 282762.884473: tracing_mark_write: qwertyuiopqwrtyuiopqwertyuiopqwertyuiopqwer[...]
//             echo-6908  ( 6908) [000] ...1 282762.884492: tracing_mark_write:
// clang-format on

static ExamplePage g_really_long_event{
    "synthetic",
    R"(
      00000000: 6be0 48dd 2b01 0100 e403 0000 0000 0000  k.H.+...........
      00000010: 1e00 0000 0000 0000 0000 0000 c003 0000  ................
      00000020: 0500 0001 fc1a 0000 4096 3615 9cff ffff  ........@.6.....
      00000030: 7177 6572 7479 7569 6f70 7177 7274 7975  qwertyuiopqwrtyu
      00000040: 696f 7071 7765 7274 7975 696f 7071 7765  iopqwertyuiopqwe
      00000050: 7274 7975 696f 7071 7765 7274 7975 696f  rtyuiopqwertyuio
      00000060: 7071 7772 7479 7569 6f70 7177 6572 7479  pqwrtyuiopqwerty
      00000070: 7569 6f70 7177 6572 7479 7569 6f71 7765  uiopqwertyuioqwe
      00000080: 7274 7975 696f 7071 7772 7479 7569 6f70  rtyuiopqwrtyuiop
      00000090: 7177 6572 7479 7569 6f70 7177 6572 7479  qwertyuiopqwerty
      000000a0: 7569 6f71 7765 7274 7975 696f 7071 7772  uioqwertyuiopqwr
      000000b0: 7479 7569 6f70 7177 6572 7479 7569 6f70  tyuiopqwertyuiop
      000000c0: 7177 6572 7479 7569 6f70 7070 7177 6572  qwertyuiopppqwer
      000000d0: 7479 7569 6f70 7177 7274 7975 696f 7071  tyuiopqwrtyuiopq
      000000e0: 7765 7274 7975 696f 7071 7765 7274 7975  wertyuiopqwertyu
      000000f0: 696f 7071 7765 7274 7975 696f 7071 7772  iopqwertyuiopqwr
      00000100: 7479 7569 6f70 7177 6572 7479 7569 6f70  tyuiopqwertyuiop
      00000110: 7177 6572 7479 7569 6f71 7765 7274 7975  qwertyuioqwertyu
      00000120: 696f 7071 7772 7479 7569 6f70 7177 6572  iopqwrtyuiopqwer
      00000130: 7479 7569 6f70 7177 6572 7479 7569 6f71  tyuiopqwertyuioq
      00000140: 7765 7274 7975 696f 7071 7772 7479 7569  wertyuiopqwrtyui
      00000150: 6f70 7177 6572 7479 7569 6f70 7177 6572  opqwertyuiopqwer
      00000160: 7479 7569 6f70 7070 7177 6572 7479 7569  tyuiopppqwertyui
      00000170: 6f70 7177 7274 7975 696f 7071 7765 7274  opqwrtyuiopqwert
      00000180: 7975 696f 7071 7765 7274 7975 696f 7071  yuiopqwertyuiopq
      00000190: 7765 7274 7975 696f 7071 7772 7479 7569  wertyuiopqwrtyui
      000001a0: 6f70 7177 6572 7479 7569 6f70 7177 6572  opqwertyuiopqwer
      000001b0: 7479 7569 6f71 7765 7274 7975 696f 7071  tyuioqwertyuiopq
      000001c0: 7772 7479 7569 6f70 7177 6572 7479 7569  wrtyuiopqwertyui
      000001d0: 6f70 7177 6572 7479 7569 6f71 7765 7274  opqwertyuioqwert
      000001e0: 7975 696f 7071 7772 7479 7569 6f70 7177  yuiopqwrtyuiopqw
      000001f0: 6572 7479 7569 6f70 7177 6572 7479 7569  ertyuiopqwertyui
      00000200: 6f70 7070 7177 6572 7479 7569 6f70 7177  opppqwertyuiopqw
      00000210: 7274 7975 696f 7071 7765 7274 7975 696f  rtyuiopqwertyuio
      00000220: 7071 7765 7274 7975 696f 7071 7765 7274  pqwertyuiopqwert
      00000230: 7975 696f 7071 7772 7479 7569 6f70 7177  yuiopqwrtyuiopqw
      00000240: 6572 7479 7569 6f70 7177 6572 7479 7569  ertyuiopqwertyui
      00000250: 6f71 7765 7274 7975 696f 7071 7772 7479  oqwertyuiopqwrty
      00000260: 7569 6f70 7177 6572 7479 7569 6f70 7177  uiopqwertyuiopqw
      00000270: 6572 7479 7569 6f71 7765 7274 7975 696f  ertyuioqwertyuio
      00000280: 7071 7772 7479 7569 6f70 7177 6572 7479  pqwrtyuiopqwerty
      00000290: 7569 6f70 7177 6572 7479 7569 6f70 7070  uiopqwertyuioppp
      000002a0: 7177 6572 7479 7569 6f70 7177 7274 7975  qwertyuiopqwrtyu
      000002b0: 696f 7071 7765 7274 7975 696f 7071 7765  iopqwertyuiopqwe
      000002c0: 7274 7975 696f 7071 7765 7274 7975 696f  rtyuiopqwertyuio
      000002d0: 7071 7772 7479 7569 6f70 7177 6572 7479  pqwrtyuiopqwerty
      000002e0: 7569 6f70 7177 6572 7479 7569 6f71 7765  uiopqwertyuioqwe
      000002f0: 7274 7975 696f 7071 7772 7479 7569 6f70  rtyuiopqwrtyuiop
      00000300: 7177 6572 7479 7569 6f70 7177 6572 7479  qwertyuiopqwerty
      00000310: 7569 6f71 7765 7274 7975 696f 7071 7772  uioqwertyuiopqwr
      00000320: 7479 7569 6f70 7177 6572 7479 7569 6f70  tyuiopqwertyuiop
      00000330: 7177 6572 7479 7569 6f70 7070 7177 6572  qwertyuiopppqwer
      00000340: 7479 7569 6f70 7177 7274 7975 696f 7071  tyuiopqwrtyuiopq
      00000350: 7765 7274 7975 696f 7071 7765 7274 7975  wertyuiopqwertyu
      00000360: 696f 7071 7765 7274 7975 696f 7071 7772  iopqwertyuiopqwr
      00000370: 7479 7569 6f70 7177 6572 7479 7569 6f70  tyuiopqwertyuiop
      00000380: 7177 6572 7479 7569 6f71 7765 7274 7975  qwertyuioqwertyu
      00000390: 696f 7071 7772 7479 7569 6f70 7177 6572  iopqwrtyuiopqwer
      000003a0: 7479 7569 6f70 7177 6572 7479 7569 6f71  tyuiopqwertyuioq
      000003b0: 7765 7274 7975 696f 7071 7772 7479 7569  wertyuiopqwrtyui
      000003c0: 6f70 7177 6572 7479 7569 6f70 7177 6572  opqwertyuiopqwer
      000003d0: 7479 7569 6f70 7070 0a00 5115 6562 0900  tyuioppp..Q.eb..
      000003e0: 0500 0001 fc1a 0000 4096 3615 9cff ffff  ........@.6.....
      000003f0: 0a00 0000 0000 0000 0000 0000 0000 0000  ................
      00000400: 0000 0000 0000 0000 0000 0000 0000 0000  ................
      00000410: 0000 0000 0000 0000 0000 0000 0000 0000  ................
      00000420: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  )",
};

TEST(CpuReaderTest, ReallyLongEvent) {
  const ExamplePage* test_case = &g_really_long_event;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("ftrace", "print")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  CpuReader::ParsePagePayload(parse_pos, &page_header.value(), table,
                              &ds_config, &compact_buffer,
                              bundle_provider.writer(), &metadata);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  const protos::gen::FtraceEvent& long_print = bundle->event()[0];
  EXPECT_THAT(long_print.print().buf(), StartsWith("qwerty"));
  EXPECT_THAT(long_print.print().buf(), EndsWith("ppp\n"));
  const protos::gen::FtraceEvent& newline = bundle->event()[1];
  EXPECT_EQ(newline.print().buf(), "\n");
}

// This event is as the event for ParseSinglePrint above except the string
// is extended to overflow the page size written in the header.
static ExamplePage g_single_print_malformed{
    "synthetic",
    R"(
    00000000: ba12 6a33 c628 0200 2c00 0000 0000 0000  ................
    00000010: def0 ec67 8d21 0000 0800 0000 0500 0001  ................
    00000020: 2870 0000 ac5d 1661 86ff ffff 4865 6c6c  ................
    00000030: 6f2c 2077 6f72 6c64 2120 776f 726c 6421  ................
    00000040: 0a00 ff00 0000 0000 0000 0000 0000 0000  ................
  )",
};

TEST(CpuReaderTest, ParseSinglePrintMalformed) {
  const ExamplePage* test_case = &g_single_print_malformed;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("ftrace", "print")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  ASSERT_EQ(0u, evt_bytes);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  ASSERT_EQ(bundle->event().size(), 1u);
  // Although one field is malformed we still see data for the rest
  // since we write the fields as we parse them for speed.
  const protos::gen::FtraceEvent& event = bundle->event()[0];
  EXPECT_EQ(event.pid(), 28712ul);
  EXPECT_TRUE(WithinOneMicrosecond(event.timestamp(), 608934, 535199));
  EXPECT_EQ(event.print().buf(), "");
}

TEST(CpuReaderTest, FilterByEvent) {
  const ExamplePage* test_case = &g_single_print;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_LT(0u, evt_bytes);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  ASSERT_EQ(bundle->event().size(), 0u);
}

// clang-format off
// # tracer: nop
// #
// # entries-in-buffer/entries-written: 3/3   #P:8
// #
// #                              _-----=> irqs-off
// #                             / _----=> need-resched
// #                            | / _---=> hardirq/softirq
// #                            || / _--=> preempt-depth
// #                            ||| /     delay
// #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
// #              | |       |   ||||       |         |
//               sh-30693 [000] ...1 615436.216806: tracing_mark_write: Hello, world!
//               sh-30693 [000] ...1 615486.377232: tracing_mark_write: Good afternoon, world!
//               sh-30693 [000] ...1 615495.632679: tracing_mark_write: Goodbye, world!
// clang-format on

static ExamplePage g_three_prints{
    "synthetic",
    R"(
    00000000: a3ab 1569 bc2f 0200 9400 0000 0000 0000  ...i./..........
    00000010: 1e00 0000 0000 0000 0800 0000 0500 0001  ................
    00000020: e577 0000 ac5d 1661 86ff ffff 4865 6c6c  .w...].a....Hell
    00000030: 6f2c 2077 6f72 6c64 210a 0000 5e32 6bb9  o, world!...^2k.
    00000040: 7501 0000 0b00 0000 0500 0001 e577 0000  u............w..
    00000050: ac5d 1661 86ff ffff 476f 6f64 2061 6674  .].a....Good aft
    00000060: 6572 6e6f 6f6e 2c20 776f 726c 6421 0a00  ernoon, world!..
    00000070: 0000 0000 9e6a 5df5 4400 0000 0900 0000  .....j].D.......
    00000080: 0500 0001 e577 0000 ac5d 1661 86ff ffff  .....w...].a....
    00000090: 476f 6f64 6279 652c 2077 6f72 6c64 210a  Goodbye, world!.
    000000a0: 0051 0000 0000 0000 0000 0000 0000 0000  .Q..............
  )",
};

TEST(CpuReaderTest, ParseThreePrint) {
  const ExamplePage* test_case = &g_three_prints;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("ftrace", "print")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_LT(0u, evt_bytes);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  ASSERT_EQ(bundle->event().size(), 3u);

  {
    const protos::gen::FtraceEvent& event = bundle->event()[0];
    EXPECT_EQ(event.pid(), 30693ul);
    EXPECT_TRUE(WithinOneMicrosecond(event.timestamp(), 615436, 216806));
    EXPECT_EQ(event.print().buf(), "Hello, world!\n");
  }

  {
    const protos::gen::FtraceEvent& event = bundle->event()[1];
    EXPECT_EQ(event.pid(), 30693ul);
    EXPECT_TRUE(WithinOneMicrosecond(event.timestamp(), 615486, 377232));
    EXPECT_EQ(event.print().buf(), "Good afternoon, world!\n");
  }

  {
    const protos::gen::FtraceEvent& event = bundle->event()[2];
    EXPECT_EQ(event.pid(), 30693ul);
    EXPECT_TRUE(WithinOneMicrosecond(event.timestamp(), 615495, 632679));
    EXPECT_EQ(event.print().buf(), "Goodbye, world!\n");
  }
}

// clang-format off
// # tracer: nop
// #
// # entries-in-buffer/entries-written: 6/6   #P:8
// #
// #                              _-----=> irqs-off
// #                             / _----=> need-resched
// #                            | / _---=> hardirq/softirq
// #                            || / _--=> preempt-depth
// #                            ||| /     delay
// #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
// #              | |       |   ||||       |         |
//      ksoftirqd/0-3     [000] d..3 1045157.722134: sched_switch: prev_comm=ksoftirqd/0 prev_pid=3 prev_prio=120 prev_state=S ==> next_comm=sleep next_pid=3733 next_prio=120
//            sleep-3733  [000] d..3 1045157.725035: sched_switch: prev_comm=sleep prev_pid=3733 prev_prio=120 prev_state=R+ ==> next_comm=rcuop/0 next_pid=10 next_prio=120
//      rcu_preempt-7     [000] d..3 1045157.725182: sched_switch: prev_comm=rcu_preempt prev_pid=7 prev_prio=120 prev_state=S ==> next_comm=sleep next_pid=3733 next_prio=120
//            sleep-3733  [000] d..3 1045157.725671: sched_switch: prev_comm=sleep prev_pid=3733 prev_prio=120 prev_state=R+ ==> next_comm=sh next_pid=3513 next_prio=120
//               sh-3513  [000] d..3 1045157.726668: sched_switch: prev_comm=sh prev_pid=3513 prev_prio=120 prev_state=S ==> next_comm=sleep next_pid=3733 next_prio=120
//            sleep-3733  [000] d..3 1045157.726697: sched_switch: prev_comm=sleep prev_pid=3733 prev_prio=120 prev_state=x ==> next_comm=kworker/u16:3 next_pid=3681 next_prio=120
// clang-format on

static ExamplePage g_six_sched_switch{
    "synthetic",
    R"(
    00000000: 2b16 c3be 90b6 0300 a001 0000 0000 0000  +...............
    00000010: 1e00 0000 0000 0000 1000 0000 2f00 0103  ............/...
    00000020: 0300 0000 6b73 6f66 7469 7271 642f 3000  ....ksoftirqd/0.
    00000030: 0000 0000 0300 0000 7800 0000 0100 0000  ........x.......
    00000040: 0000 0000 736c 6565 7000 722f 3000 0000  ....sleep.r/0...
    00000050: 0000 0000 950e 0000 7800 0000 b072 8805  ........x....r..
    00000060: 2f00 0103 950e 0000 736c 6565 7000 722f  /.......sleep.r/
    00000070: 3000 0000 0000 0000 950e 0000 7800 0000  0...........x...
    00000080: 0008 0000 0000 0000 7263 756f 702f 3000  ........rcuop/0.
    00000090: 0000 0000 0000 0000 0a00 0000 7800 0000  ............x...
    000000a0: f0b0 4700 2f00 0103 0700 0000 7263 755f  ..G./.......rcu_
    000000b0: 7072 6565 6d70 7400 0000 0000 0700 0000  preempt.........
    000000c0: 7800 0000 0100 0000 0000 0000 736c 6565  x...........slee
    000000d0: 7000 722f 3000 0000 0000 0000 950e 0000  p.r/0...........
    000000e0: 7800 0000 1001 ef00 2f00 0103 950e 0000  x......./.......
    000000f0: 736c 6565 7000 722f 3000 0000 0000 0000  sleep.r/0.......
    00000100: 950e 0000 7800 0000 0008 0000 0000 0000  ....x...........
    00000110: 7368 0064 0065 722f 3000 0000 0000 0000  sh.d.er/0.......
    00000120: b90d 0000 7800 0000 f0c7 e601 2f00 0103  ....x......./...
    00000130: b90d 0000 7368 0064 0065 722f 3000 0000  ....sh.d.er/0...
    00000140: 0000 0000 b90d 0000 7800 0000 0100 0000  ........x.......
    00000150: 0000 0000 736c 6565 7000 722f 3000 0000  ....sleep.r/0...
    00000160: 0000 0000 950e 0000 7800 0000 d030 0e00  ........x....0..
    00000170: 2f00 0103 950e 0000 736c 6565 7000 722f  /.......sleep.r/
    00000180: 3000 0000 0000 0000 950e 0000 7800 0000  0...........x...
    00000190: 4000 0000 0000 0000 6b77 6f72 6b65 722f  @.......kworker/
    000001a0: 7531 363a 3300 0000 610e 0000 7800 0000  u16:3...a...x...
    000001b0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
    )",
};

TEST(CpuReaderTest, ParseSixSchedSwitch) {
  const ExamplePage* test_case = &g_six_sched_switch;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("sched", "sched_switch")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_LT(0u, evt_bytes);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  ASSERT_EQ(bundle->event().size(), 6u);

  {
    const protos::gen::FtraceEvent& event = bundle->event()[1];
    EXPECT_EQ(event.pid(), 3733ul);
    EXPECT_TRUE(WithinOneMicrosecond(event.timestamp(), 1045157, 725035));
    EXPECT_EQ(event.sched_switch().prev_comm(), "sleep");
    EXPECT_EQ(event.sched_switch().prev_pid(), 3733);
    EXPECT_EQ(event.sched_switch().prev_prio(), 120);
    EXPECT_EQ(event.sched_switch().next_comm(), "rcuop/0");
    EXPECT_EQ(event.sched_switch().next_pid(), 10);
    EXPECT_EQ(event.sched_switch().next_prio(), 120);
  }
}

TEST(CpuReaderTest, ParseSixSchedSwitchCompactFormat) {
  const ExamplePage* test_case = &g_six_sched_switch;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config{
      EventFilter{}, EnabledCompactSchedConfigForTesting(), {}, {}};
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("sched", "sched_switch")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_LT(0u, evt_bytes);

  // Nothing written into the proto yet:
  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  EXPECT_EQ(0u, bundle->event().size());
  EXPECT_FALSE(bundle->has_compact_sched());

  // Instead, sched switch fields were buffered:
  EXPECT_LT(0u, compact_buffer.sched_switch().size());
  EXPECT_LT(0u, compact_buffer.interner().interned_comms_size());

  // Write the buffer out & check the serialized format:
  compact_buffer.WriteAndReset(bundle_provider.writer());
  bundle_provider.writer()->Finalize();
  bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);

  const auto& compact_sched = bundle->compact_sched();

  EXPECT_EQ(6u, compact_sched.switch_timestamp().size());
  EXPECT_EQ(6u, compact_sched.switch_prev_state().size());
  EXPECT_EQ(6u, compact_sched.switch_next_pid().size());
  EXPECT_EQ(6u, compact_sched.switch_next_prio().size());
  // 4 unique interned next_comm strings:
  EXPECT_EQ(4u, compact_sched.intern_table().size());
  EXPECT_EQ(6u, compact_sched.switch_next_comm_index().size());

  // First event exactly as expected (absolute timestamp):
  EXPECT_TRUE(WithinOneMicrosecond(compact_sched.switch_timestamp()[0], 1045157,
                                   722134));
  EXPECT_EQ(1, compact_sched.switch_prev_state()[0]);
  EXPECT_EQ(3733, compact_sched.switch_next_pid()[0]);
  EXPECT_EQ(120, compact_sched.switch_next_prio()[0]);
  auto comm_intern_idx = compact_sched.switch_next_comm_index()[0];
  std::string next_comm = compact_sched.intern_table()[comm_intern_idx];
  EXPECT_EQ("sleep", next_comm);
}

TEST_F(CpuReaderTableTest, ParseAllFields) {
  using FakeEventProvider =
      ProtoProvider<pbzero::FakeFtraceEvent, gen::FakeFtraceEvent>;

  uint16_t ftrace_event_id = 102;

  std::vector<Field> common_fields;
  {
    common_fields.emplace_back(Field{});
    Field* field = &common_fields.back();
    field->ftrace_offset = 0;
    field->ftrace_size = 4;
    field->ftrace_type = kFtraceUint32;
    field->proto_field_id = 1;
    field->proto_field_type = ProtoSchemaType::kUint32;
    SetTranslationStrategy(field->ftrace_type, field->proto_field_type,
                           &field->strategy);
  }

  {
    common_fields.emplace_back(Field{});
    Field* field = &common_fields.back();
    field->ftrace_offset = 4;
    field->ftrace_size = 4;
    field->ftrace_type = kFtraceCommonPid32;
    field->proto_field_id = 2;
    field->proto_field_type = ProtoSchemaType::kInt32;
    SetTranslationStrategy(field->ftrace_type, field->proto_field_type,
                           &field->strategy);
  }

  std::vector<Event> events;
  events.emplace_back(Event{});
  {
    Event* event = &events.back();
    event->name = "";
    event->group = "";
    event->proto_field_id = 42;
    event->ftrace_event_id = ftrace_event_id;

    {
      // uint32 -> uint32
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 8;
      field->ftrace_size = 4;
      field->ftrace_type = kFtraceUint32;
      field->proto_field_id = 1;
      field->proto_field_type = ProtoSchemaType::kUint32;
    }

    {
      // pid32 -> uint32
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 12;
      field->ftrace_size = 4;
      field->ftrace_type = kFtracePid32;
      field->proto_field_id = 2;
      field->proto_field_type = ProtoSchemaType::kInt32;
    }

    {
      // dev32 -> uint64
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 16;
      field->ftrace_size = 4;
      field->ftrace_type = kFtraceDevId32;
      field->proto_field_id = 3;
      field->proto_field_type = ProtoSchemaType::kUint64;
    }

    {
      // ino_t (32bit) -> uint64
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 20;
      field->ftrace_size = 4;
      field->ftrace_type = kFtraceInode32;
      field->proto_field_id = 4;
      field->proto_field_type = ProtoSchemaType::kUint64;
    }

    {
      // dev64 -> uint64
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 24;
      field->ftrace_size = 8;
      field->ftrace_type = kFtraceDevId64;
      field->proto_field_id = 5;
      field->proto_field_type = ProtoSchemaType::kUint64;
    }

    {
      // ino_t (64bit) -> uint64
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 32;
      field->ftrace_size = 8;
      field->ftrace_type = kFtraceInode64;
      field->proto_field_id = 6;
      field->proto_field_type = ProtoSchemaType::kUint64;
    }

    {
      // char[16] -> string
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 40;
      field->ftrace_size = 16;
      field->ftrace_type = kFtraceFixedCString;
      field->proto_field_id = 500;
      field->proto_field_type = ProtoSchemaType::kString;
    }

    {
      // dataloc -> string
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 57;
      field->ftrace_size = 4;
      field->ftrace_type = kFtraceDataLoc;
      field->proto_field_id = 502;
      field->proto_field_type = ProtoSchemaType::kString;
    }

    {
      // char -> string
      event->fields.emplace_back(Field{});
      Field* field = &event->fields.back();
      field->ftrace_offset = 61;
      field->ftrace_size = 0;
      field->ftrace_type = kFtraceCString;
      field->proto_field_id = 501;
      field->proto_field_type = ProtoSchemaType::kString;
    }

    for (Field& field : event->fields) {
      SetTranslationStrategy(field.ftrace_type, field.proto_field_type,
                             &field.strategy);
    }
  }

  ProtoTranslationTable table(
      &ftrace_, events, std::move(common_fields),
      ProtoTranslationTable::DefaultPageHeaderSpecForTesting(),
      InvalidCompactSchedEventFormatForTesting());

  FakeEventProvider provider(base::kPageSize);

  BinaryWriter writer;

  // Must use the bit masks to translate between kernel and userspace device ids
  // to generate the below examples
  const uint32_t kKernelBlockDeviceId = 271581216;

  const BlockDeviceID kUserspaceBlockDeviceId =
      CpuReader::TranslateBlockDeviceIDToUserspace<BlockDeviceID>(
          kKernelBlockDeviceId);
  const uint64_t k64BitKernelBlockDeviceId = 4442450946;
  const BlockDeviceID k64BitUserspaceBlockDeviceId =
      CpuReader::TranslateBlockDeviceIDToUserspace<uint64_t>(
          k64BitKernelBlockDeviceId);

  writer.Write<int32_t>(1001);                       // Common field.
  writer.Write<int32_t>(9999);                       // Common pid
  writer.Write<int32_t>(1003);                       // Uint32 field
  writer.Write<int32_t>(97);                         // Pid
  writer.Write<int32_t>(kKernelBlockDeviceId);       // Dev id
  writer.Write<int32_t>(98);                         // Inode 32
  writer.Write<int64_t>(k64BitKernelBlockDeviceId);  // Dev id 64
  writer.Write<int64_t>(99u);                        // Inode 64
  writer.WriteFixedString(16, "Hello");
  writer.Write<uint8_t>(0);  // Deliberately mis-aligning.
  writer.Write<uint32_t>(40 | 6 << 16);
  writer.WriteFixedString(300, "Goodbye");

  auto input = writer.GetCopy();
  auto length = writer.written();
  FtraceMetadata metadata{};

  ASSERT_TRUE(CpuReader::ParseEvent(ftrace_event_id, input.get(),
                                    input.get() + length, &table,
                                    provider.writer(), &metadata));

  auto event = provider.ParseProto();
  ASSERT_TRUE(event);
  EXPECT_EQ(event->common_field(), 1001ul);
  EXPECT_EQ(event->common_pid(), 9999ul);
  EXPECT_TRUE(event->has_all_fields());
  EXPECT_EQ(event->all_fields().field_uint32(), 1003u);
  EXPECT_EQ(event->all_fields().field_pid(), 97);
  EXPECT_EQ(event->all_fields().field_dev_32(),
            static_cast<uint32_t>(kUserspaceBlockDeviceId));
  EXPECT_EQ(event->all_fields().field_inode_32(), 98u);
// TODO(primiano): for some reason this fails on mac.
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
  EXPECT_EQ(event->all_fields().field_dev_64(), k64BitUserspaceBlockDeviceId);
#endif
  EXPECT_EQ(event->all_fields().field_inode_64(), 99u);
  EXPECT_EQ(event->all_fields().field_char_16(), "Hello");
  EXPECT_EQ(event->all_fields().field_char(), "Goodbye");
  EXPECT_EQ(event->all_fields().field_data_loc(), "Hello");
  EXPECT_THAT(metadata.pids, Contains(97));
  EXPECT_EQ(metadata.inode_and_device.size(), 2U);
  EXPECT_THAT(metadata.inode_and_device,
              Contains(Pair(98u, kUserspaceBlockDeviceId)));
  EXPECT_THAT(metadata.inode_and_device,
              Contains(Pair(99u, k64BitUserspaceBlockDeviceId)));
}

TEST(CpuReaderTest, TaskRenameEvent) {
  BundleProvider bundle_provider(base::kPageSize);

  BinaryWriter writer;
  ProtoTranslationTable* table = GetTable("android_seed_N2F62_3.10.49");

  constexpr uint32_t kTaskRenameId = 19;

  writer.Write<int32_t>(1001);             // Common field.
  writer.Write<int32_t>(9999);             // Common pid
  writer.Write<int32_t>(9999);             // Pid
  writer.WriteFixedString(16, "Hello");    // Old Comm
  writer.WriteFixedString(16, "Goodbye");  // New Comm
  writer.Write<uint64_t>(10);              // flags
  writer.Write<int16_t>(10);               // oom_score_adj

  auto input = writer.GetCopy();
  auto length = writer.written();
  FtraceMetadata metadata{};

  ASSERT_TRUE(CpuReader::ParseEvent(kTaskRenameId, input.get(),
                                    input.get() + length, table,
                                    bundle_provider.writer(), &metadata));
  EXPECT_THAT(metadata.rename_pids, Contains(9999));
  EXPECT_THAT(metadata.pids, Contains(9999));
}

// Page with a single sched_switch, no data loss.
static char g_switch_page[] =
    R"(
    00000000: 2b16 c3be 90b6 0300 4c00 0000 0000 0000  ................
    00000010: 1e00 0000 0000 0000 1000 0000 2f00 0103  ................
    00000020: 0300 0000 6b73 6f66 7469 7271 642f 3000  ................
    00000030: 0000 0000 0300 0000 7800 0000 0100 0000  ................
    00000040: 0000 0000 736c 6565 7000 722f 3000 0000  ................
    00000050: 0000 0000 950e 0000 7800 0000 0000 0000  ................
    )";

// Page with a single sched_switch, header has data loss flag set.
static char g_switch_page_lost_events[] =
    R"(
    00000000: 2b16 c3be 90b6 0300 4c00 0080 ffff ffff  ................
    00000010: 1e00 0000 0000 0000 1000 0000 2f00 0103  ................
    00000020: 0300 0000 6b73 6f66 7469 7271 642f 3000  ................
    00000030: 0000 0000 0300 0000 7800 0000 0100 0000  ................
    00000040: 0000 0000 736c 6565 7000 722f 3000 0000  ................
    00000050: 0000 0000 950e 0000 7800 0000 0000 0000  ................
    )";

TEST(CpuReaderTest, NewPacketOnLostEvents) {
  auto page_ok = PageFromXxd(g_switch_page);
  auto page_loss = PageFromXxd(g_switch_page_lost_events);

  std::vector<const void*> test_page_order = {
      page_ok.get(),   page_ok.get(), page_ok.get(), page_loss.get(),
      page_loss.get(), page_ok.get(), page_ok.get(), page_ok.get()};

  // Prepare a buffer with 8 contiguous pages, with the above contents.
  static constexpr size_t kTestPages = 8;
  uint8_t buf[base::kPageSize * kTestPages] = {};
  for (size_t i = 0; i < kTestPages; i++) {
    void* dest = buf + (i * base::kPageSize);
    memcpy(dest, static_cast<const void*>(test_page_order[i]), base::kPageSize);
  }

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable("synthetic");
  FtraceMetadata metadata{};
  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("sched", "sched_switch")));

  TraceWriterForTesting trace_writer;
  CpuReader::ProcessPagesForDataSource(&trace_writer, &metadata, /*cpu=*/1,
                                       &ds_config, buf, kTestPages, table);

  // Each packet should contain the parsed contents of a contiguous run of pages
  // without data loss.
  // So we should get three packets (each page has 1 event):
  //   [3 events] [1 event] [4 events].
  auto packets = trace_writer.GetAllTracePackets();

  ASSERT_EQ(3u, packets.size());
  EXPECT_FALSE(packets[0].ftrace_events().lost_events());
  EXPECT_EQ(3u, packets[0].ftrace_events().event().size());

  EXPECT_TRUE(packets[1].ftrace_events().lost_events());
  EXPECT_EQ(1u, packets[1].ftrace_events().event().size());

  EXPECT_TRUE(packets[2].ftrace_events().lost_events());
  EXPECT_EQ(4u, packets[2].ftrace_events().event().size());
}

TEST(CpuReaderTest, TranslateBlockDeviceIDToUserspace) {
  const uint32_t kKernelBlockDeviceId = 271581216;
  const BlockDeviceID kUserspaceBlockDeviceId = 66336;
  const uint64_t k64BitKernelBlockDeviceId = 4442450946;
  const BlockDeviceID k64BitUserspaceBlockDeviceId =
      static_cast<BlockDeviceID>(17594983681026ULL);

  EXPECT_EQ(CpuReader::TranslateBlockDeviceIDToUserspace<uint32_t>(
                kKernelBlockDeviceId),
            kUserspaceBlockDeviceId);
  EXPECT_EQ(CpuReader::TranslateBlockDeviceIDToUserspace<uint64_t>(
                k64BitKernelBlockDeviceId),
            k64BitUserspaceBlockDeviceId);
}

// clang-format off
// # tracer: nop
// #
// # entries-in-buffer/entries-written: 1041/238740   #P:8
// #
// #                              _-----=> irqs-off
// #                             / _----=> need-resched
// #                            | / _---=> hardirq/softirq
// #                            || / _--=> preempt-depth
// #                            ||| /     delay
// #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
// #              | |       |   ||||       |         |
//       android.bg-1668  [000] ...1 174991.234105: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234108: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234118: ext4_da_write_begin: dev 259,32 ino 2883605 pos 20480 len 4096 flags 0
//       android.bg-1668  [000] ...1 174991.234126: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//       android.bg-1668  [000] ...1 174991.234133: ext4_es_lookup_extent_enter: dev 259,32 ino 2883605 lblk 5
//       android.bg-1668  [000] ...1 174991.234135: ext4_es_lookup_extent_exit: dev 259,32 ino 2883605 found 1 [5/4294967290) 576460752303423487 H0x10
//       android.bg-1668  [000] ...2 174991.234140: ext4_da_reserve_space: dev 259,32 ino 2883605 mode 0100600 i_blocks 8 reserved_data_blocks 6 reserved_meta_blocks 0
//       android.bg-1668  [000] ...1 174991.234142: ext4_es_insert_extent: dev 259,32 ino 2883605 es [5/1) mapped 576460752303423487 status D
//       android.bg-1668  [000] ...1 174991.234153: ext4_da_write_end: dev 259,32 ino 2883605 pos 20480 len 4096 copied 4096
//       android.bg-1668  [000] ...1 174991.234158: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234160: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234170: ext4_da_write_begin: dev 259,32 ino 2883605 pos 24576 len 2968 flags 0
//       android.bg-1668  [000] ...1 174991.234178: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//       android.bg-1668  [000] ...1 174991.234184: ext4_es_lookup_extent_enter: dev 259,32 ino 2883605 lblk 6
//       android.bg-1668  [000] ...1 174991.234187: ext4_es_lookup_extent_exit: dev 259,32 ino 2883605 found 1 [6/4294967289) 576460752303423487 H0x10
//       android.bg-1668  [000] ...2 174991.234191: ext4_da_reserve_space: dev 259,32 ino 2883605 mode 0100600 i_blocks 8 reserved_data_blocks 7 reserved_meta_blocks 0
//       android.bg-1668  [000] ...1 174991.234193: ext4_es_insert_extent: dev 259,32 ino 2883605 es [6/1) mapped 576460752303423487 status D
//       android.bg-1668  [000] ...1 174991.234203: ext4_da_write_end: dev 259,32 ino 2883605 pos 24576 len 2968 copied 2968
//       android.bg-1668  [000] ...1 174991.234209: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234211: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234262: ext4_sync_file_enter: dev 259,32 ino 2883605 parent 2883592 datasync 0
//       android.bg-1668  [000] ...1 174991.234270: ext4_writepages: dev 259,32 ino 2883605 nr_to_write 9223372036854775807 pages_skipped 0 range_start 0 range_end 9223372036854775807 sync_mode 1 for_kupdate 0 range_cyclic 0 writeback_index 0
//       android.bg-1668  [000] ...1 174991.234287: ext4_journal_start: dev 259,32 blocks, 10 rsv_blocks, 0 caller ext4_writepages+0x6a4/0x119c
//       android.bg-1668  [000] ...1 174991.234294: ext4_da_write_pages: dev 259,32 ino 2883605 first_page 0 nr_to_write 9223372036854775807 sync_mode 1
//       android.bg-1668  [000] ...1 174991.234319: ext4_da_write_pages_extent: dev 259,32 ino 2883605 lblk 0 len 7 flags 0x200
//       android.bg-1668  [000] ...1 174991.234322: ext4_es_lookup_extent_enter: dev 259,32 ino 2883605 lblk 0
//       android.bg-1668  [000] ...1 174991.234324: ext4_es_lookup_extent_exit: dev 259,32 ino 2883605 found 1 [0/7) 576460752303423487 D0x10
//       android.bg-1668  [000] ...1 174991.234328: ext4_ext_map_blocks_enter: dev 259,32 ino 2883605 lblk 0 len 7 flags CREATE|DELALLOC|METADATA_NOFAIL
//       android.bg-1668  [000] ...1 174991.234341: ext4_request_blocks: dev 259,32 ino 2883605 flags HINT_DATA|DELALLOC_RESV|USE_RESV len 7 lblk 0 goal 11567104 lleft 0 lright 0 pleft 0 pright 0
//       android.bg-1668  [000] ...1 174991.234394: ext4_mballoc_prealloc: dev 259,32 inode 2883605 orig 353/0/7@0 result 65/25551/7@0
//       android.bg-1668  [000] ...1 174991.234400: ext4_allocate_blocks: dev 259,32 ino 2883605 flags HINT_DATA|DELALLOC_RESV|USE_RESV len 7 block 2155471 lblk 0 goal 11567104 lleft 0 lright 0 pleft 0 pright 0
//       android.bg-1668  [000] ...1 174991.234409: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller __ext4_ext_dirty+0x104/0x170
//       android.bg-1668  [000] ...1 174991.234420: ext4_get_reserved_cluster_alloc: dev 259,32 ino 2883605 lblk 0 len 7
//       android.bg-1668  [000] ...2 174991.234426: ext4_da_update_reserve_space: dev 259,32 ino 2883605 mode 0100600 i_blocks 8 used_blocks 7 reserved_data_blocks 7 reserved_meta_blocks 0 allocated_meta_blocks 0 quota_claim 1
//       android.bg-1668  [000] ...1 174991.234434: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//       android.bg-1668  [000] ...1 174991.234441: ext4_es_lookup_extent_enter: dev 259,32 ino 3 lblk 1
//       android.bg-1668  [000] ...1 174991.234445: ext4_es_lookup_extent_exit: dev 259,32 ino 3 found 1 [0/2) 9255 W0x10
//       android.bg-1668  [000] ...1 174991.234456: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//       android.bg-1668  [000] ...1 174991.234460: ext4_es_lookup_extent_enter: dev 259,32 ino 4 lblk 1
//       android.bg-1668  [000] ...1 174991.234463: ext4_es_lookup_extent_exit: dev 259,32 ino 4 found 1 [0/2) 9257 W0x10
//       android.bg-1668  [000] ...1 174991.234471: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234474: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234481: ext4_ext_map_blocks_exit: dev 259,32 ino 2883605 flags CREATE|DELALLOC|METADATA_NOFAIL lblk 0 pblk 2155471 len 7 mflags NM ret 7
//       android.bg-1668  [000] ...1 174991.234484: ext4_es_insert_extent: dev 259,32 ino 2883605 es [0/7) mapped 2155471 status W
//       android.bg-1668  [000] ...1 174991.234547: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_writepages+0xdc0/0x119c
//       android.bg-1668  [000] ...1 174991.234604: ext4_journal_start: dev 259,32 blocks, 10 rsv_blocks, 0 caller ext4_writepages+0x6a4/0x119c
//       android.bg-1668  [000] ...1 174991.234609: ext4_da_write_pages: dev 259,32 ino 2883605 first_page 7 nr_to_write 9223372036854775800 sync_mode 1
//       android.bg-1668  [000] ...1 174991.234876: ext4_writepages_result: dev 259,32 ino 2883605 ret 0 pages_written 7 pages_skipped 0 sync_mode 1 writeback_index 7
//    Profile Saver-5504  [000] ...1 175002.711928: ext4_discard_preallocations: dev 259,32 ino 1311176
//    Profile Saver-5504  [000] ...1 175002.714165: ext4_begin_ordered_truncate: dev 259,32 ino 1311176 new_size 0
//    Profile Saver-5504  [000] ...1 175002.714172: ext4_journal_start: dev 259,32 blocks, 3 rsv_blocks, 0 caller ext4_setattr+0x5b4/0x788
//    Profile Saver-5504  [000] ...1 175002.714218: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_setattr+0x65c/0x788
//    Profile Saver-5504  [000] ...1 175002.714277: ext4_invalidatepage: dev 259,32 ino 1311176 page_index 0 offset 0 length 4096
//    Profile Saver-5504  [000] ...1 175002.714281: ext4_releasepage: dev 259,32 ino 1311176 page_index 0
//    Profile Saver-5504  [000] ...1 175002.714295: ext4_invalidatepage: dev 259,32 ino 1311176 page_index 1 offset 0 length 4096
//    Profile Saver-5504  [000] ...1 175002.714296: ext4_releasepage: dev 259,32 ino 1311176 page_index 1
//    Profile Saver-5504  [000] ...1 175002.714315: ext4_truncate_enter: dev 259,32 ino 1311176 blocks 24
//    Profile Saver-5504  [000] ...1 175002.714318: ext4_journal_start: dev 259,32 blocks, 10 rsv_blocks, 0 caller ext4_truncate+0x258/0x4b8
//    Profile Saver-5504  [000] ...1 175002.714322: ext4_discard_preallocations: dev 259,32 ino 1311176
//    Profile Saver-5504  [000] ...1 175002.714324: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_ext_truncate+0x24/0xc8
//    Profile Saver-5504  [000] ...1 175002.714328: ext4_es_remove_extent: dev 259,32 ino 1311176 es [0/4294967295)
//    Profile Saver-5504  [000] ...1 175002.714335: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_ext_remove_space+0x60/0x1180
//    Profile Saver-5504  [000] ...1 175002.714338: ext4_ext_remove_space: dev 259,32 ino 1311176 since 0 end 4294967294 depth 0
//    Profile Saver-5504  [000] ...1 175002.714347: ext4_ext_rm_leaf: dev 259,32 ino 1311176 start_lblk 0 last_extent [0(5276994), 2]partial_cluster 0
//    Profile Saver-5504  [000] ...1 175002.714351: ext4_remove_blocks: dev 259,32 ino 1311176 extent [0(5276994), 2]from 0 to 1 partial_cluster 0
//    Profile Saver-5504  [000] ...1 175002.714354: ext4_free_blocks: dev 259,32 ino 1311176 mode 0100600 block 5276994 count 2 flags 1ST_CLUSTER
//    Profile Saver-5504  [000] ...1 175002.714365: ext4_mballoc_free: dev 259,32 inode 1311176 extent 161/1346/2
//    Profile Saver-5504  [000] ...1 175002.714382: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//    Profile Saver-5504  [000] ...1 175002.714391: ext4_es_lookup_extent_enter: dev 259,32 ino 3 lblk 4
//    Profile Saver-5504  [000] ...1 175002.714394: ext4_es_lookup_extent_exit: dev 259,32 ino 3 found 1 [4/1) 557094 W0x10
//    Profile Saver-5504  [000] ...1 175002.714402: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//    Profile Saver-5504  [000] ...1 175002.714404: ext4_es_lookup_extent_enter: dev 259,32 ino 4 lblk 8
//    Profile Saver-5504  [000] ...1 175002.714406: ext4_es_lookup_extent_exit: dev 259,32 ino 4 found 1 [8/3) 7376914 W0x10
//    Profile Saver-5504  [000] ...1 175002.714413: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714414: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.714420: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller __ext4_ext_dirty+0x104/0x170
//    Profile Saver-5504  [000] ...1 175002.714423: ext4_ext_remove_space_done: dev 259,32 ino 1311176 since 0 end 4294967294 depth 0 partial 0 remaining_entries 0
//    Profile Saver-5504  [000] ...1 175002.714425: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller __ext4_ext_dirty+0x104/0x170
//    Profile Saver-5504  [000] ...1 175002.714433: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_truncate+0x3c4/0x4b8
//    Profile Saver-5504  [000] ...1 175002.714436: ext4_truncate_exit: dev 259,32 ino 1311176 blocks 8
//    Profile Saver-5504  [000] ...1 175002.714437: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714438: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.714462: ext4_da_write_begin: dev 259,32 ino 1311176 pos 0 len 4 flags 0
//    Profile Saver-5504  [000] ...1 175002.714472: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.714477: ext4_es_lookup_extent_enter: dev 259,32 ino 1311176 lblk 0
//    Profile Saver-5504  [000] ...1 175002.714477: ext4_es_lookup_extent_exit: dev 259,32 ino 1311176 found 0 [0/0) 0
//    Profile Saver-5504  [000] ...1 175002.714480: ext4_ext_map_blocks_enter: dev 259,32 ino 1311176 lblk 0 len 1 flags
//    Profile Saver-5504  [000] ...1 175002.714485: ext4_es_find_delayed_extent_range_enter: dev 259,32 ino 1311176 lblk 0
//    Profile Saver-5504  [000] ...1 175002.714488: ext4_es_find_delayed_extent_range_exit: dev 259,32 ino 1311176 es [0/0) mapped 0 status
//    Profile Saver-5504  [000] ...1 175002.714490: ext4_es_insert_extent: dev 259,32 ino 1311176 es [0/4294967295) mapped 576460752303423487 status H
//    Profile Saver-5504  [000] ...1 175002.714495: ext4_ext_map_blocks_exit: dev 259,32 ino 1311176 flags  lblk 0 pblk 4294967296 len 1 mflags  ret 0
//    Profile Saver-5504  [000] ...2 175002.714501: ext4_da_reserve_space: dev 259,32 ino 1311176 mode 0100600 i_blocks 8 reserved_data_blocks 1 reserved_meta_blocks 0
//    Profile Saver-5504  [000] ...1 175002.714505: ext4_es_insert_extent: dev 259,32 ino 1311176 es [0/1) mapped 576460752303423487 status D
//    Profile Saver-5504  [000] ...1 175002.714513: ext4_da_write_end: dev 259,32 ino 1311176 pos 0 len 4 copied 4
//    Profile Saver-5504  [000] ...1 175002.714519: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714520: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.714527: ext4_da_write_begin: dev 259,32 ino 1311176 pos 4 len 4 flags 0
//    Profile Saver-5504  [000] ...1 175002.714529: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.714531: ext4_da_write_end: dev 259,32 ino 1311176 pos 4 len 4 copied 4
//    Profile Saver-5504  [000] ...1 175002.714532: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714532: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.715313: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.715322: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.723849: ext4_da_write_begin: dev 259,32 ino 1311176 pos 8 len 5 flags 0
//    Profile Saver-5504  [000] ...1 175002.723862: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.723873: ext4_da_write_end: dev 259,32 ino 1311176 pos 8 len 5 copied 5
//    Profile Saver-5504  [000] ...1 175002.723877: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.723879: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726857: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726867: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726881: ext4_da_write_begin: dev 259,32 ino 1311176 pos 13 len 4 flags 0
//    Profile Saver-5504  [000] ...1 175002.726883: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.726890: ext4_da_write_end: dev 259,32 ino 1311176 pos 13 len 4 copied 4
//    Profile Saver-5504  [000] ...1 175002.726892: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726892: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726900: ext4_da_write_begin: dev 259,32 ino 1311176 pos 17 len 4079 flags 0
//    Profile Saver-5504  [000] ...1 175002.726901: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.726904: ext4_da_write_end: dev 259,32 ino 1311176 pos 17 len 4079 copied 4079
//    Profile Saver-5504  [000] ...1 175002.726905: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726906: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726908: ext4_da_write_begin: dev 259,32 ino 1311176 pos 4096 len 2780 flags 0
//    Profile Saver-5504  [000] ...1 175002.726916: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.726921: ext4_es_lookup_extent_enter: dev 259,32 ino 1311176 lblk 1
//    Profile Saver-5504  [000] ...1 175002.726924: ext4_es_lookup_extent_exit: dev 259,32 ino 1311176 found 1 [1/4294967294) 576460752303423487 H0x10
//    Profile Saver-5504  [000] ...2 175002.726931: ext4_da_reserve_space: dev 259,32 ino 1311176 mode 0100600 i_blocks 8 reserved_data_blocks 2 reserved_meta_blocks 0
//    Profile Saver-5504  [000] ...1 175002.726933: ext4_es_insert_extent: dev 259,32 ino 1311176 es [1/1) mapped 576460752303423487 status D
//    Profile Saver-5504  [000] ...1 175002.726940: ext4_da_write_end: dev 259,32 ino 1311176 pos 4096 len 2780 copied 2780
//    Profile Saver-5504  [000] ...1 175002.726941: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726942: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//   d.process.acor-27885 [000] ...1 175018.227675: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//   d.process.acor-27885 [000] ...1 175018.227699: ext4_mark_inode_dirty: dev 259,32 ino 3278189 caller ext4_dirty_inode+0x48/0x68
//   d.process.acor-27885 [000] ...1 175018.227839: ext4_sync_file_enter: dev 259,32 ino 3278183 parent 3277001 datasync 1
//   d.process.acor-27885 [000] ...1 175018.227847: ext4_writepages: dev 259,32 ino 3278183 nr_to_write 9223372036854775807 pages_skipped 0 range_start 0 range_end 9223372036854775807 sync_mode 1 for_kupdate 0 range_cyclic 0 writeback_index 2
//   d.process.acor-27885 [000] ...1 175018.227852: ext4_writepages_result: dev 259,32 ino 3278183 ret 0 pages_written 0 pages_skipped 0 sync_mode 1 writeback_index 2
// clang-format on

static ExamplePage g_full_page_sched_switch{
    "synthetic",
    R"(
00000000: 31f2 7622 1a00 0000 b40f 0000 0000 0000  1.v"............
00000010: 1e00 0000 0000 0000 1000 0000 2f00 0103  ............/...
00000020: 140d 0000 4a69 7420 7468 7265 6164 2070  ....Jit thread p
00000030: 6f6f 6c00 140d 0000 8100 0000 0008 0000  ool.............
00000040: 0000 0000 4576 656e 7454 6872 6561 6400  ....EventThread.
00000050: 6572 0000 7002 0000 6100 0000 f057 0e00  er..p...a....W..
00000060: 2f00 0103 7002 0000 4576 656e 7454 6872  /...p...EventThr
00000070: 6561 6400 6572 0000 7002 0000 6100 0000  ead.er..p...a...
00000080: 0100 0000 0000 0000 4a69 7420 7468 7265  ........Jit thre
00000090: 6164 2070 6f6f 6c00 140d 0000 8100 0000  ad pool.........
000000a0: 50c2 0910 2f00 0103 140d 0000 4a69 7420  P.../.......Jit
000000b0: 7468 7265 6164 2070 6f6f 6c00 140d 0000  thread pool.....
000000c0: 8100 0000 0100 0000 0000 0000 7377 6170  ............swap
000000d0: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
000000e0: 7800 0000 901a c80e 2f00 0103 0000 0000  x......./.......
000000f0: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000100: 0000 0000 7800 0000 0000 0000 0000 0000  ....x...........
00000110: 4469 7370 5379 6e63 0069 6e67 6572 0000  DispSync.inger..
00000120: 6f02 0000 6100 0000 1064 1e00 2f00 0103  o...a....d../...
00000130: 6f02 0000 4469 7370 5379 6e63 0069 6e67  o...DispSync.ing
00000140: 6572 0000 6f02 0000 6100 0000 0100 0000  er..o...a.......
00000150: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000160: 0000 0000 0000 0000 7800 0000 9074 8600  ........x....t..
00000170: 2f00 0103 0000 0000 7377 6170 7065 722f  /.......swapper/
00000180: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000190: 0000 0000 0000 0000 4576 656e 7454 6872  ........EventThr
000001a0: 6561 6400 6572 0000 7002 0000 6100 0000  ead.er..p...a...
000001b0: d071 0b00 2f00 0103 7002 0000 4576 656e  .q../...p...Even
000001c0: 7454 6872 6561 6400 6572 0000 7002 0000  tThread.er..p...
000001d0: 6100 0000 0100 0000 0000 0000 7377 6170  a...........swap
000001e0: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
000001f0: 7800 0000 10cd 4504 2f00 0103 0000 0000  x.....E./.......
00000200: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000210: 0000 0000 7800 0000 0000 0000 0000 0000  ....x...........
00000220: 7375 676f 763a 3000 0000 0000 0000 0000  sugov:0.........
00000230: 3802 0000 3100 0000 30d6 1300 2f00 0103  8...1...0.../...
00000240: 3802 0000 7375 676f 763a 3000 0000 0000  8...sugov:0.....
00000250: 0000 0000 3802 0000 3100 0000 0100 0000  ....8...1.......
00000260: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000270: 0000 0000 0000 0000 7800 0000 3049 a202  ........x...0I..
00000280: 2f00 0103 0000 0000 7377 6170 7065 722f  /.......swapper/
00000290: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
000002a0: 0000 0000 0000 0000 4469 7370 5379 6e63  ........DispSync
000002b0: 0069 6e67 6572 0000 6f02 0000 6100 0000  .inger..o...a...
000002c0: d07a 1000 2f00 0103 6f02 0000 4469 7370  .z../...o...Disp
000002d0: 5379 6e63 0069 6e67 6572 0000 6f02 0000  Sync.inger..o...
000002e0: 6100 0000 0100 0000 0000 0000 7377 6170  a...........swap
000002f0: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000300: 7800 0000 d085 1100 2f00 0103 0000 0000  x......./.......
00000310: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000320: 0000 0000 7800 0000 0000 0000 0000 0000  ....x...........
00000330: 7375 7266 6163 6566 6c69 6e67 6572 0000  surfaceflinger..
00000340: 4b02 0000 6200 0000 907a f000 2f00 0103  K...b....z../...
00000350: 4b02 0000 7375 7266 6163 6566 6c69 6e67  K...surfacefling
00000360: 6572 0000 4b02 0000 6200 0000 0100 0000  er..K...b.......
00000370: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000380: 0000 0000 0000 0000 7800 0000 305a 6400  ........x...0Zd.
00000390: 2f00 0103 0000 0000 7377 6170 7065 722f  /.......swapper/
000003a0: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
000003b0: 0000 0000 0000 0000 6d64 7373 5f66 6230  ........mdss_fb0
000003c0: 0000 0000 0000 0000 5714 0000 5300 0000  ........W...S...
000003d0: 10b1 9e03 2f00 0103 5714 0000 6d64 7373  ..../...W...mdss
000003e0: 5f66 6230 0000 0000 0000 0000 5714 0000  _fb0........W...
000003f0: 5300 0000 0200 0000 0000 0000 6b73 6f66  S...........ksof
00000400: 7469 7271 642f 3000 0000 0000 0300 0000  tirqd/0.........
00000410: 7800 0000 90bb 9900 2f00 0103 0300 0000  x......./.......
00000420: 6b73 6f66 7469 7271 642f 3000 0000 0000  ksoftirqd/0.....
00000430: 0300 0000 7800 0000 0100 0000 0000 0000  ....x...........
00000440: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000450: 0000 0000 7800 0000 701e 5305 2f00 0103  ....x...p.S./...
00000460: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000470: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000480: 0000 0000 6b77 6f72 6b65 722f 7531 363a  ....kworker/u16:
00000490: 3600 0000 6401 0000 7800 0000 90a1 2900  6...d...x.....).
000004a0: 2f00 0103 6401 0000 6b77 6f72 6b65 722f  /...d...kworker/
000004b0: 7531 363a 3600 0000 6401 0000 7800 0000  u16:6...d...x...
000004c0: 0200 0000 0000 0000 7377 6170 7065 722f  ........swapper/
000004d0: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
000004e0: b0e5 4f04 2f00 0103 0000 0000 7377 6170  ..O./.......swap
000004f0: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000500: 7800 0000 0000 0000 0000 0000 4269 6e64  x...........Bind
00000510: 6572 3a32 3136 385f 3135 0000 e614 0000  er:2168_15......
00000520: 7800 0000 b0bd 7c00 2f00 0103 e614 0000  x.....|./.......
00000530: 4269 6e64 6572 3a32 3136 385f 3135 0000  Binder:2168_15..
00000540: e614 0000 7800 0000 0100 0000 0000 0000  ....x...........
00000550: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000560: 0000 0000 7800 0000 d0bd 7e01 2f00 0103  ....x.....~./...
00000570: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000580: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000590: 0000 0000 6b77 6f72 6b65 722f 7531 363a  ....kworker/u16:
000005a0: 3900 0000 e204 0000 7800 0000 7016 0800  9.......x...p...
000005b0: 2f00 0103 e204 0000 6b77 6f72 6b65 722f  /.......kworker/
000005c0: 7531 363a 3900 0000 e204 0000 7800 0000  u16:9.......x...
000005d0: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
000005e0: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
000005f0: 1004 5200 2f00 0103 0000 0000 7377 6170  ..R./.......swap
00000600: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000610: 7800 0000 0000 0000 0000 0000 6b77 6f72  x...........kwor
00000620: 6b65 722f 7531 363a 3900 0000 e204 0000  ker/u16:9.......
00000630: 7800 0000 d0db 0700 2f00 0103 e204 0000  x......./.......
00000640: 6b77 6f72 6b65 722f 7531 363a 3900 0000  kworker/u16:9...
00000650: e204 0000 7800 0000 0100 0000 0000 0000  ....x...........
00000660: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000670: 0000 0000 7800 0000 b0a2 8c00 2f00 0103  ....x......./...
00000680: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000690: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
000006a0: 0000 0000 6b77 6f72 6b65 722f 7531 363a  ....kworker/u16:
000006b0: 3900 0000 e204 0000 7800 0000 d02b 0400  9.......x....+..
000006c0: 2f00 0103 e204 0000 6b77 6f72 6b65 722f  /.......kworker/
000006d0: 7531 363a 3900 0000 e204 0000 7800 0000  u16:9.......x...
000006e0: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
000006f0: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000700: d064 ef05 2f00 0103 0000 0000 7377 6170  .d../.......swap
00000710: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000720: 7800 0000 0000 0000 0000 0000 4469 7370  x...........Disp
00000730: 5379 6e63 0069 6e67 6572 0000 6f02 0000  Sync.inger..o...
00000740: 6100 0000 f07d 1b00 2f00 0103 6f02 0000  a....}../...o...
00000750: 4469 7370 5379 6e63 0069 6e67 6572 0000  DispSync.inger..
00000760: 6f02 0000 6100 0000 0100 0000 0000 0000  o...a...........
00000770: 6b73 6f66 7469 7271 642f 3000 0000 0000  ksoftirqd/0.....
00000780: 0300 0000 7800 0000 304c 2000 2f00 0103  ....x...0L ./...
00000790: 0300 0000 6b73 6f66 7469 7271 642f 3000  ....ksoftirqd/0.
000007a0: 0000 0000 0300 0000 7800 0000 0100 0000  ........x.......
000007b0: 0000 0000 6465 7832 6f61 7400 3935 5f33  ....dex2oat.95_3
000007c0: 0000 0000 341f 0000 8200 0000 700b 0700  ....4.......p...
000007d0: 2f00 0103 341f 0000 6465 7832 6f61 7400  /...4...dex2oat.
000007e0: 3935 5f33 0000 0000 341f 0000 8200 0000  95_3....4.......
000007f0: 0000 0000 0000 0000 7375 676f 763a 3000  ........sugov:0.
00000800: 0000 0000 0000 0000 3802 0000 3100 0000  ........8...1...
00000810: 50b0 0600 2f00 0103 3802 0000 7375 676f  P.../...8...sugo
00000820: 763a 3000 0000 0000 0000 0000 3802 0000  v:0.........8...
00000830: 3100 0000 0008 0000 0000 0000 6d69 6772  1...........migr
00000840: 6174 696f 6e2f 3000 0000 0000 0d00 0000  ation/0.........
00000850: 0000 0000 d09c 0600 2f00 0103 0d00 0000  ......../.......
00000860: 6d69 6772 6174 696f 6e2f 3000 0000 0000  migration/0.....
00000870: 0d00 0000 0000 0000 0100 0000 0000 0000  ................
00000880: 7375 676f 763a 3000 0000 0000 0000 0000  sugov:0.........
00000890: 3802 0000 3100 0000 7061 1900 2f00 0103  8...1...pa../...
000008a0: 3802 0000 7375 676f 763a 3000 0000 0000  8...sugov:0.....
000008b0: 0000 0000 3802 0000 3100 0000 0100 0000  ....8...1.......
000008c0: 0000 0000 6465 7832 6f61 7400 3935 5f33  ....dex2oat.95_3
000008d0: 0000 0000 341f 0000 8200 0000 f03c 5600  ....4........<V.
000008e0: 2f00 0103 341f 0000 6465 7832 6f61 7400  /...4...dex2oat.
000008f0: 3935 5f33 0000 0000 341f 0000 8200 0000  95_3....4.......
00000900: 0200 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000910: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000920: 5013 c400 2f00 0103 0000 0000 7377 6170  P.../.......swap
00000930: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000940: 7800 0000 0000 0000 0000 0000 616e 6472  x...........andr
00000950: 6f69 642e 6861 7264 7761 7200 d20a 0000  oid.hardwar.....
00000960: 7800 0000 30c9 1300 2f00 0103 d20a 0000  x...0.../.......
00000970: 616e 6472 6f69 642e 6861 7264 7761 7200  android.hardwar.
00000980: d20a 0000 7800 0000 0100 0000 0000 0000  ....x...........
00000990: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
000009a0: 0000 0000 7800 0000 7097 c000 2f00 0103  ....x...p.../...
000009b0: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
000009c0: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
000009d0: 0000 0000 616e 6472 6f69 642e 6861 7264  ....android.hard
000009e0: 7761 7200 d20a 0000 7800 0000 305c 0c00  war.....x...0\..
000009f0: 2f00 0103 d20a 0000 616e 6472 6f69 642e  /.......android.
00000a00: 6861 7264 7761 7200 d20a 0000 7800 0000  hardwar.....x...
00000a10: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000a20: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000a30: d0aa 1401 2f00 0103 0000 0000 7377 6170  ..../.......swap
00000a40: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000a50: 7800 0000 0000 0000 0000 0000 616e 6472  x...........andr
00000a60: 6f69 642e 6861 7264 7761 7200 d20a 0000  oid.hardwar.....
00000a70: 7800 0000 903b 0c00 2f00 0103 d20a 0000  x....;../.......
00000a80: 616e 6472 6f69 642e 6861 7264 7761 7200  android.hardwar.
00000a90: d20a 0000 7800 0000 0100 0000 0000 0000  ....x...........
00000aa0: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000ab0: 0000 0000 7800 0000 f024 5401 2f00 0103  ....x....$T./...
00000ac0: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000ad0: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000ae0: 0000 0000 616e 6472 6f69 642e 6861 7264  ....android.hard
00000af0: 7761 7200 d20a 0000 7800 0000 f0f3 0b00  war.....x.......
00000b00: 2f00 0103 d20a 0000 616e 6472 6f69 642e  /.......android.
00000b10: 6861 7264 7761 7200 d20a 0000 7800 0000  hardwar.....x...
00000b20: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000b30: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000b40: d0b5 bf02 2f00 0103 0000 0000 7377 6170  ..../.......swap
00000b50: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000b60: 7800 0000 0000 0000 0000 0000 4469 7370  x...........Disp
00000b70: 5379 6e63 0069 6e67 6572 0000 6f02 0000  Sync.inger..o...
00000b80: 6100 0000 90cd 1400 2f00 0103 6f02 0000  a......./...o...
00000b90: 4469 7370 5379 6e63 0069 6e67 6572 0000  DispSync.inger..
00000ba0: 6f02 0000 6100 0000 0100 0000 0000 0000  o...a...........
00000bb0: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000bc0: 0000 0000 7800 0000 50a6 1100 2f00 0103  ....x...P.../...
00000bd0: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000be0: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000bf0: 0000 0000 7375 7266 6163 6566 6c69 6e67  ....surfacefling
00000c00: 6572 0000 4b02 0000 6200 0000 b04c 4200  er..K...b....LB.
00000c10: 2f00 0103 4b02 0000 7375 7266 6163 6566  /...K...surfacef
00000c20: 6c69 6e67 6572 0000 4b02 0000 6200 0000  linger..K...b...
00000c30: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000c40: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000c50: b025 060a 2f00 0103 0000 0000 7377 6170  .%../.......swap
00000c60: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000c70: 7800 0000 0000 0000 0000 0000 6b77 6f72  x...........kwor
00000c80: 6b65 722f 7531 363a 3600 0000 6401 0000  ker/u16:6...d...
00000c90: 7800 0000 d0b6 0600 2f00 0103 6401 0000  x......./...d...
00000ca0: 6b77 6f72 6b65 722f 7531 363a 3600 0000  kworker/u16:6...
00000cb0: 6401 0000 7800 0000 0100 0000 0000 0000  d...x...........
00000cc0: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000cd0: 0000 0000 7800 0000 f0a0 5800 2f00 0103  ....x.....X./...
00000ce0: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000cf0: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000d00: 0000 0000 6b77 6f72 6b65 722f 7531 363a  ....kworker/u16:
00000d10: 3600 0000 6401 0000 7800 0000 f07a 1300  6...d...x....z..
00000d20: 2f00 0103 6401 0000 6b77 6f72 6b65 722f  /...d...kworker/
00000d30: 7531 363a 3600 0000 6401 0000 7800 0000  u16:6...d...x...
00000d40: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000d50: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000d60: b080 b101 2f00 0103 0000 0000 7377 6170  ..../.......swap
00000d70: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000d80: 7800 0000 0000 0000 0000 0000 6b77 6f72  x...........kwor
00000d90: 6b65 722f 7531 363a 3600 0000 6401 0000  ker/u16:6...d...
00000da0: 7800 0000 103c 1200 2f00 0103 6401 0000  x....<../...d...
00000db0: 6b77 6f72 6b65 722f 7531 363a 3600 0000  kworker/u16:6...
00000dc0: 6401 0000 7800 0000 0100 0000 0000 0000  d...x...........
00000dd0: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000de0: 0000 0000 7800 0000 50ea 3800 2f00 0103  ....x...P.8./...
00000df0: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000e00: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000e10: 0000 0000 6b77 6f72 6b65 722f 7531 363a  ....kworker/u16:
00000e20: 3600 0000 6401 0000 7800 0000 5032 0400  6...d...x...P2..
00000e30: 2f00 0103 6401 0000 6b77 6f72 6b65 722f  /...d...kworker/
00000e40: 7531 363a 3600 0000 6401 0000 7800 0000  u16:6...d...x...
00000e50: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000e60: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000e70: 70f5 9000 2f00 0103 0000 0000 7377 6170  p.../.......swap
00000e80: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000e90: 7800 0000 0000 0000 0000 0000 6b77 6f72  x...........kwor
00000ea0: 6b65 722f 7531 363a 3600 0000 6401 0000  ker/u16:6...d...
00000eb0: 7800 0000 10d7 0300 2f00 0103 6401 0000  x......./...d...
00000ec0: 6b77 6f72 6b65 722f 7531 363a 3600 0000  kworker/u16:6...
00000ed0: 6401 0000 7800 0000 0100 0000 0000 0000  d...x...........
00000ee0: 7377 6170 7065 722f 3000 0000 0000 0000  swapper/0.......
00000ef0: 0000 0000 7800 0000 907c 0900 2f00 0103  ....x....|../...
00000f00: 0000 0000 7377 6170 7065 722f 3000 0000  ....swapper/0...
00000f10: 0000 0000 0000 0000 7800 0000 0000 0000  ........x.......
00000f20: 0000 0000 6b77 6f72 6b65 722f 7531 363a  ....kworker/u16:
00000f30: 3600 0000 6401 0000 7800 0000 7082 0300  6...d...x...p...
00000f40: 2f00 0103 6401 0000 6b77 6f72 6b65 722f  /...d...kworker/
00000f50: 7531 363a 3600 0000 6401 0000 7800 0000  u16:6...d...x...
00000f60: 0100 0000 0000 0000 7377 6170 7065 722f  ........swapper/
00000f70: 3000 0000 0000 0000 0000 0000 7800 0000  0...........x...
00000f80: f0ec 2100 2f00 0103 0000 0000 7377 6170  ..!./.......swap
00000f90: 7065 722f 3000 0000 0000 0000 0000 0000  per/0...........
00000fa0: 7800 0000 0000 0000 0000 0000 6b77 6f72  x...........kwor
00000fb0: 6b65 722f 7531 363a 3600 0000 6401 0000  ker/u16:6...d...
00000fc0: 7800 0000 0000 0000 0000 0000 0000 0000  x...............
00000fd0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000fe0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000ff0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
    )",
};

TEST(CpuReaderTest, ParseFullPageSchedSwitch) {
  const ExamplePage* test_case = &g_full_page_sched_switch;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("sched", "sched_switch")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_FALSE(page_header->lost_events);
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_LT(0u, evt_bytes);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
  EXPECT_EQ(bundle->event().size(), 59u);
}

// clang-format off
// # tracer: nop
// #
// # entries-in-buffer/entries-written: 1041/238740   #P:8
// #
// #                              _-----=> irqs-off
// #                             / _----=> need-resched
// #                            | / _---=> hardirq/softirq
// #                            || / _--=> preempt-depth
// #                            ||| /     delay
// #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
// #              | |       |   ||||       |         |
//       android.bg-1668  [000] ...1 174991.234105: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234108: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234118: ext4_da_write_begin: dev 259,32 ino 2883605 pos 20480 len 4096 flags 0
//       android.bg-1668  [000] ...1 174991.234126: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//       android.bg-1668  [000] ...1 174991.234133: ext4_es_lookup_extent_enter: dev 259,32 ino 2883605 lblk 5
//       android.bg-1668  [000] ...1 174991.234135: ext4_es_lookup_extent_exit: dev 259,32 ino 2883605 found 1 [5/4294967290) 576460752303423487 H0x10
//       android.bg-1668  [000] ...2 174991.234140: ext4_da_reserve_space: dev 259,32 ino 2883605 mode 0100600 i_blocks 8 reserved_data_blocks 6 reserved_meta_blocks 0
//       android.bg-1668  [000] ...1 174991.234142: ext4_es_insert_extent: dev 259,32 ino 2883605 es [5/1) mapped 576460752303423487 status D
//       android.bg-1668  [000] ...1 174991.234153: ext4_da_write_end: dev 259,32 ino 2883605 pos 20480 len 4096 copied 4096
//       android.bg-1668  [000] ...1 174991.234158: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234160: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234170: ext4_da_write_begin: dev 259,32 ino 2883605 pos 24576 len 2968 flags 0
//       android.bg-1668  [000] ...1 174991.234178: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//       android.bg-1668  [000] ...1 174991.234184: ext4_es_lookup_extent_enter: dev 259,32 ino 2883605 lblk 6
//       android.bg-1668  [000] ...1 174991.234187: ext4_es_lookup_extent_exit: dev 259,32 ino 2883605 found 1 [6/4294967289) 576460752303423487 H0x10
//       android.bg-1668  [000] ...2 174991.234191: ext4_da_reserve_space: dev 259,32 ino 2883605 mode 0100600 i_blocks 8 reserved_data_blocks 7 reserved_meta_blocks 0
//       android.bg-1668  [000] ...1 174991.234193: ext4_es_insert_extent: dev 259,32 ino 2883605 es [6/1) mapped 576460752303423487 status D
//       android.bg-1668  [000] ...1 174991.234203: ext4_da_write_end: dev 259,32 ino 2883605 pos 24576 len 2968 copied 2968
//       android.bg-1668  [000] ...1 174991.234209: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234211: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234262: ext4_sync_file_enter: dev 259,32 ino 2883605 parent 2883592 datasync 0
//       android.bg-1668  [000] ...1 174991.234270: ext4_writepages: dev 259,32 ino 2883605 nr_to_write 9223372036854775807 pages_skipped 0 range_start 0 range_end 9223372036854775807 sync_mode 1 for_kupdate 0 range_cyclic 0 writeback_index 0
//       android.bg-1668  [000] ...1 174991.234287: ext4_journal_start: dev 259,32 blocks, 10 rsv_blocks, 0 caller ext4_writepages+0x6a4/0x119c
//       android.bg-1668  [000] ...1 174991.234294: ext4_da_write_pages: dev 259,32 ino 2883605 first_page 0 nr_to_write 9223372036854775807 sync_mode 1
//       android.bg-1668  [000] ...1 174991.234319: ext4_da_write_pages_extent: dev 259,32 ino 2883605 lblk 0 len 7 flags 0x200
//       android.bg-1668  [000] ...1 174991.234322: ext4_es_lookup_extent_enter: dev 259,32 ino 2883605 lblk 0
//       android.bg-1668  [000] ...1 174991.234324: ext4_es_lookup_extent_exit: dev 259,32 ino 2883605 found 1 [0/7) 576460752303423487 D0x10
//       android.bg-1668  [000] ...1 174991.234328: ext4_ext_map_blocks_enter: dev 259,32 ino 2883605 lblk 0 len 7 flags CREATE|DELALLOC|METADATA_NOFAIL
//       android.bg-1668  [000] ...1 174991.234341: ext4_request_blocks: dev 259,32 ino 2883605 flags HINT_DATA|DELALLOC_RESV|USE_RESV len 7 lblk 0 goal 11567104 lleft 0 lright 0 pleft 0 pright 0
//       android.bg-1668  [000] ...1 174991.234394: ext4_mballoc_prealloc: dev 259,32 inode 2883605 orig 353/0/7@0 result 65/25551/7@0
//       android.bg-1668  [000] ...1 174991.234400: ext4_allocate_blocks: dev 259,32 ino 2883605 flags HINT_DATA|DELALLOC_RESV|USE_RESV len 7 block 2155471 lblk 0 goal 11567104 lleft 0 lright 0 pleft 0 pright 0
//       android.bg-1668  [000] ...1 174991.234409: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller __ext4_ext_dirty+0x104/0x170
//       android.bg-1668  [000] ...1 174991.234420: ext4_get_reserved_cluster_alloc: dev 259,32 ino 2883605 lblk 0 len 7
//       android.bg-1668  [000] ...2 174991.234426: ext4_da_update_reserve_space: dev 259,32 ino 2883605 mode 0100600 i_blocks 8 used_blocks 7 reserved_data_blocks 7 reserved_meta_blocks 0 allocated_meta_blocks 0 quota_claim 1
//       android.bg-1668  [000] ...1 174991.234434: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//       android.bg-1668  [000] ...1 174991.234441: ext4_es_lookup_extent_enter: dev 259,32 ino 3 lblk 1
//       android.bg-1668  [000] ...1 174991.234445: ext4_es_lookup_extent_exit: dev 259,32 ino 3 found 1 [0/2) 9255 W0x10
//       android.bg-1668  [000] ...1 174991.234456: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//       android.bg-1668  [000] ...1 174991.234460: ext4_es_lookup_extent_enter: dev 259,32 ino 4 lblk 1
//       android.bg-1668  [000] ...1 174991.234463: ext4_es_lookup_extent_exit: dev 259,32 ino 4 found 1 [0/2) 9257 W0x10
//       android.bg-1668  [000] ...1 174991.234471: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//       android.bg-1668  [000] ...1 174991.234474: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_dirty_inode+0x48/0x68
//       android.bg-1668  [000] ...1 174991.234481: ext4_ext_map_blocks_exit: dev 259,32 ino 2883605 flags CREATE|DELALLOC|METADATA_NOFAIL lblk 0 pblk 2155471 len 7 mflags NM ret 7
//       android.bg-1668  [000] ...1 174991.234484: ext4_es_insert_extent: dev 259,32 ino 2883605 es [0/7) mapped 2155471 status W
//       android.bg-1668  [000] ...1 174991.234547: ext4_mark_inode_dirty: dev 259,32 ino 2883605 caller ext4_writepages+0xdc0/0x119c
//       android.bg-1668  [000] ...1 174991.234604: ext4_journal_start: dev 259,32 blocks, 10 rsv_blocks, 0 caller ext4_writepages+0x6a4/0x119c
//       android.bg-1668  [000] ...1 174991.234609: ext4_da_write_pages: dev 259,32 ino 2883605 first_page 7 nr_to_write 9223372036854775800 sync_mode 1
//       android.bg-1668  [000] ...1 174991.234876: ext4_writepages_result: dev 259,32 ino 2883605 ret 0 pages_written 7 pages_skipped 0 sync_mode 1 writeback_index 7
//    Profile Saver-5504  [000] ...1 175002.711928: ext4_discard_preallocations: dev 259,32 ino 1311176
//    Profile Saver-5504  [000] ...1 175002.714165: ext4_begin_ordered_truncate: dev 259,32 ino 1311176 new_size 0
//    Profile Saver-5504  [000] ...1 175002.714172: ext4_journal_start: dev 259,32 blocks, 3 rsv_blocks, 0 caller ext4_setattr+0x5b4/0x788
//    Profile Saver-5504  [000] ...1 175002.714218: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_setattr+0x65c/0x788
//    Profile Saver-5504  [000] ...1 175002.714277: ext4_invalidatepage: dev 259,32 ino 1311176 page_index 0 offset 0 length 4096
//    Profile Saver-5504  [000] ...1 175002.714281: ext4_releasepage: dev 259,32 ino 1311176 page_index 0
//    Profile Saver-5504  [000] ...1 175002.714295: ext4_invalidatepage: dev 259,32 ino 1311176 page_index 1 offset 0 length 4096
//    Profile Saver-5504  [000] ...1 175002.714296: ext4_releasepage: dev 259,32 ino 1311176 page_index 1
//    Profile Saver-5504  [000] ...1 175002.714315: ext4_truncate_enter: dev 259,32 ino 1311176 blocks 24
//    Profile Saver-5504  [000] ...1 175002.714318: ext4_journal_start: dev 259,32 blocks, 10 rsv_blocks, 0 caller ext4_truncate+0x258/0x4b8
//    Profile Saver-5504  [000] ...1 175002.714322: ext4_discard_preallocations: dev 259,32 ino 1311176
//    Profile Saver-5504  [000] ...1 175002.714324: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_ext_truncate+0x24/0xc8
//    Profile Saver-5504  [000] ...1 175002.714328: ext4_es_remove_extent: dev 259,32 ino 1311176 es [0/4294967295)
//    Profile Saver-5504  [000] ...1 175002.714335: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_ext_remove_space+0x60/0x1180
//    Profile Saver-5504  [000] ...1 175002.714338: ext4_ext_remove_space: dev 259,32 ino 1311176 since 0 end 4294967294 depth 0
//    Profile Saver-5504  [000] ...1 175002.714347: ext4_ext_rm_leaf: dev 259,32 ino 1311176 start_lblk 0 last_extent [0(5276994), 2]partial_cluster 0
//    Profile Saver-5504  [000] ...1 175002.714351: ext4_remove_blocks: dev 259,32 ino 1311176 extent [0(5276994), 2]from 0 to 1 partial_cluster 0
//    Profile Saver-5504  [000] ...1 175002.714354: ext4_free_blocks: dev 259,32 ino 1311176 mode 0100600 block 5276994 count 2 flags 1ST_CLUSTER
//    Profile Saver-5504  [000] ...1 175002.714365: ext4_mballoc_free: dev 259,32 inode 1311176 extent 161/1346/2
//    Profile Saver-5504  [000] ...1 175002.714382: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//    Profile Saver-5504  [000] ...1 175002.714391: ext4_es_lookup_extent_enter: dev 259,32 ino 3 lblk 4
//    Profile Saver-5504  [000] ...1 175002.714394: ext4_es_lookup_extent_exit: dev 259,32 ino 3 found 1 [4/1) 557094 W0x10
//    Profile Saver-5504  [000] ...1 175002.714402: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_mark_dquot_dirty+0x80/0xd4
//    Profile Saver-5504  [000] ...1 175002.714404: ext4_es_lookup_extent_enter: dev 259,32 ino 4 lblk 8
//    Profile Saver-5504  [000] ...1 175002.714406: ext4_es_lookup_extent_exit: dev 259,32 ino 4 found 1 [8/3) 7376914 W0x10
//    Profile Saver-5504  [000] ...1 175002.714413: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714414: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.714420: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller __ext4_ext_dirty+0x104/0x170
//    Profile Saver-5504  [000] ...1 175002.714423: ext4_ext_remove_space_done: dev 259,32 ino 1311176 since 0 end 4294967294 depth 0 partial 0 remaining_entries 0
//    Profile Saver-5504  [000] ...1 175002.714425: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller __ext4_ext_dirty+0x104/0x170
//    Profile Saver-5504  [000] ...1 175002.714433: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_truncate+0x3c4/0x4b8
//    Profile Saver-5504  [000] ...1 175002.714436: ext4_truncate_exit: dev 259,32 ino 1311176 blocks 8
//    Profile Saver-5504  [000] ...1 175002.714437: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714438: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.714462: ext4_da_write_begin: dev 259,32 ino 1311176 pos 0 len 4 flags 0
//    Profile Saver-5504  [000] ...1 175002.714472: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.714477: ext4_es_lookup_extent_enter: dev 259,32 ino 1311176 lblk 0
//    Profile Saver-5504  [000] ...1 175002.714477: ext4_es_lookup_extent_exit: dev 259,32 ino 1311176 found 0 [0/0) 0
//    Profile Saver-5504  [000] ...1 175002.714480: ext4_ext_map_blocks_enter: dev 259,32 ino 1311176 lblk 0 len 1 flags
//    Profile Saver-5504  [000] ...1 175002.714485: ext4_es_find_delayed_extent_range_enter: dev 259,32 ino 1311176 lblk 0
//    Profile Saver-5504  [000] ...1 175002.714488: ext4_es_find_delayed_extent_range_exit: dev 259,32 ino 1311176 es [0/0) mapped 0 status
//    Profile Saver-5504  [000] ...1 175002.714490: ext4_es_insert_extent: dev 259,32 ino 1311176 es [0/4294967295) mapped 576460752303423487 status H
//    Profile Saver-5504  [000] ...1 175002.714495: ext4_ext_map_blocks_exit: dev 259,32 ino 1311176 flags  lblk 0 pblk 4294967296 len 1 mflags  ret 0
//    Profile Saver-5504  [000] ...2 175002.714501: ext4_da_reserve_space: dev 259,32 ino 1311176 mode 0100600 i_blocks 8 reserved_data_blocks 1 reserved_meta_blocks 0
//    Profile Saver-5504  [000] ...1 175002.714505: ext4_es_insert_extent: dev 259,32 ino 1311176 es [0/1) mapped 576460752303423487 status D
//    Profile Saver-5504  [000] ...1 175002.714513: ext4_da_write_end: dev 259,32 ino 1311176 pos 0 len 4 copied 4
//    Profile Saver-5504  [000] ...1 175002.714519: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714520: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.714527: ext4_da_write_begin: dev 259,32 ino 1311176 pos 4 len 4 flags 0
//    Profile Saver-5504  [000] ...1 175002.714529: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.714531: ext4_da_write_end: dev 259,32 ino 1311176 pos 4 len 4 copied 4
//    Profile Saver-5504  [000] ...1 175002.714532: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.714532: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.715313: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.715322: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.723849: ext4_da_write_begin: dev 259,32 ino 1311176 pos 8 len 5 flags 0
//    Profile Saver-5504  [000] ...1 175002.723862: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.723873: ext4_da_write_end: dev 259,32 ino 1311176 pos 8 len 5 copied 5
//    Profile Saver-5504  [000] ...1 175002.723877: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.723879: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726857: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726867: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726881: ext4_da_write_begin: dev 259,32 ino 1311176 pos 13 len 4 flags 0
//    Profile Saver-5504  [000] ...1 175002.726883: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.726890: ext4_da_write_end: dev 259,32 ino 1311176 pos 13 len 4 copied 4
//    Profile Saver-5504  [000] ...1 175002.726892: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726892: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726900: ext4_da_write_begin: dev 259,32 ino 1311176 pos 17 len 4079 flags 0
//    Profile Saver-5504  [000] ...1 175002.726901: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.726904: ext4_da_write_end: dev 259,32 ino 1311176 pos 17 len 4079 copied 4079
//    Profile Saver-5504  [000] ...1 175002.726905: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726906: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//    Profile Saver-5504  [000] ...1 175002.726908: ext4_da_write_begin: dev 259,32 ino 1311176 pos 4096 len 2780 flags 0
//    Profile Saver-5504  [000] ...1 175002.726916: ext4_journal_start: dev 259,32 blocks, 1 rsv_blocks, 0 caller ext4_da_write_begin+0x3d4/0x518
//    Profile Saver-5504  [000] ...1 175002.726921: ext4_es_lookup_extent_enter: dev 259,32 ino 1311176 lblk 1
//    Profile Saver-5504  [000] ...1 175002.726924: ext4_es_lookup_extent_exit: dev 259,32 ino 1311176 found 1 [1/4294967294) 576460752303423487 H0x10
//    Profile Saver-5504  [000] ...2 175002.726931: ext4_da_reserve_space: dev 259,32 ino 1311176 mode 0100600 i_blocks 8 reserved_data_blocks 2 reserved_meta_blocks 0
//    Profile Saver-5504  [000] ...1 175002.726933: ext4_es_insert_extent: dev 259,32 ino 1311176 es [1/1) mapped 576460752303423487 status D
//    Profile Saver-5504  [000] ...1 175002.726940: ext4_da_write_end: dev 259,32 ino 1311176 pos 4096 len 2780 copied 2780
//    Profile Saver-5504  [000] ...1 175002.726941: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//    Profile Saver-5504  [000] ...1 175002.726942: ext4_mark_inode_dirty: dev 259,32 ino 1311176 caller ext4_dirty_inode+0x48/0x68
//   d.process.acor-27885 [000] ...1 175018.227675: ext4_journal_start: dev 259,32 blocks, 2 rsv_blocks, 0 caller ext4_dirty_inode+0x30/0x68
//   d.process.acor-27885 [000] ...1 175018.227699: ext4_mark_inode_dirty: dev 259,32 ino 3278189 caller ext4_dirty_inode+0x48/0x68
//   d.process.acor-27885 [000] ...1 175018.227839: ext4_sync_file_enter: dev 259,32 ino 3278183 parent 3277001 datasync 1
//   d.process.acor-27885 [000] ...1 175018.227847: ext4_writepages: dev 259,32 ino 3278183 nr_to_write 9223372036854775807 pages_skipped 0 range_start 0 range_end 9223372036854775807 sync_mode 1 for_kupdate 0 range_cyclic 0 writeback_index 2
//   d.process.acor-27885 [000] ...1 175018.227852: ext4_writepages_result: dev 259,32 ino 3278183 ret 0 pages_written 0 pages_skipped 0 sync_mode 1 writeback_index 2
// clang-format on

static ExamplePage g_full_page_ext4{
    "synthetic",
    R"(
00000000: 50fe 5852 279f 0000 c80f 00c0 ffff ffff  P.XR'...........
00000010: 0800 0000 5701 0001 8406 0000 2000 3010  ....W....... .0.
00000020: 566b 0000 8829 e86a 91ff ffff 0200 0000  Vk...).j........
00000030: 0000 0000 2873 0100 1b01 0001 8406 0000  ....(s..........
00000040: 2000 3010 9200 0000 1500 2c00 0000 0000   .0.......,.....
00000050: a029 e86a 91ff ffff 0ac8 0400 1e01 0001  .).j............
00000060: 8406 0000 2000 3010 2866 0100 1500 2c00  .... .0.(f....,.
00000070: 0000 0000 0050 0000 0000 0000 0010 0000  .....P..........
00000080: 0000 0000 a804 0400 5701 0001 8406 0000  ........W.......
00000090: 2000 3010 91ff ffff 586f e86a 91ff ffff   .0.....Xo.j....
000000a0: 0100 0000 0000 0000 c83a 0300 6c01 0001  .........:..l...
000000b0: 8406 0000 2000 3010 0000 0000 1500 2c00  .... .0.......,.
000000c0: 0000 0000 0500 0000 5701 0001 ac6c 0100  ........W....l..
000000d0: 6d01 0001 8406 0000 2000 3010 91ff ffff  m....... .0.....
000000e0: 1500 2c00 0000 0000 0500 0000 faff ffff  ..,.............
000000f0: ffff ffff ffff ff07 184e 0000 0100 0000  .........N......
00000100: ec08 0200 3f01 0002 8406 0000 2000 3010  ....?....... .0.
00000110: 0000 0000 1500 2c00 0000 0000 0800 0000  ......,.........
00000120: 0000 0000 0600 0000 0000 0000 8081 0000  ................
00000130: 0000 0000 ec24 0100 6701 0001 8406 0000  .....$..g.......
00000140: 2000 3010 0000 0000 1500 2c00 0000 0000   .0.......,.....
00000150: 0500 0000 0100 0000 ffff ffff ffff ff07  ................
00000160: 0400 0000 7b04 3200 2a30 0500 2101 0001  ....{.2.*0..!...
00000170: 8406 0000 2000 3010 0000 0000 1500 2c00  .... .0.......,.
00000180: 0000 0000 0050 0000 0000 0000 0010 0000  .....P..........
00000190: 0010 0000 288b 0200 5701 0001 8406 0000  ....(...W.......
000001a0: 2000 3010 0000 0000 8829 e86a 91ff ffff   .0......).j....
000001b0: 0200 0000 0000 0000 0832 0100 1b01 0001  .........2......
000001c0: 8406 0000 2000 3010 566b 0000 1500 2c00  .... .0.Vk....,.
000001d0: 0000 0000 a029 e86a 91ff ffff eaa0 0400  .....).j........
000001e0: 1e01 0001 8406 0000 2000 3010 280b 0400  ........ .0.(...
000001f0: 1500 2c00 0000 0000 0060 0000 0000 0000  ..,......`......
00000200: 980b 0000 0000 0000 88d0 0300 5701 0001  ............W...
00000210: 8406 0000 2000 3010 566b 0000 586f e86a  .... .0.Vk..Xo.j
00000220: 91ff ffff 0100 0000 0000 0000 c813 0300  ................
00000230: 6c01 0001 8406 0000 2000 3010 566b 0000  l....... .0.Vk..
00000240: 1500 2c00 0000 0000 0600 0000 0000 0000  ..,.............
00000250: ac5f 0100 6d01 0001 8406 0000 2000 3010  ._..m....... .0.
00000260: 1100 3010 1500 2c00 0000 0000 0600 0000  ..0...,.........
00000270: f9ff ffff ffff ffff ffff ff07 185a ea6a  .............Z.j
00000280: 0100 0000 4c02 0200 3f01 0002 8406 0000  ....L...?.......
00000290: 2000 3010 566b 0000 1500 2c00 0000 0000   .0.Vk....,.....
000002a0: 0800 0000 0000 0000 0700 0000 0000 0000  ................
000002b0: 8081 0000 6d01 0001 0c0b 0100 6701 0001  ....m.......g...
000002c0: 8406 0000 2000 3010 0000 0000 1500 2c00  .... .0.......,.
000002d0: 0000 0000 0600 0000 0100 0000 ffff ffff  ................
000002e0: ffff ff07 049a 0100 5701 0001 aa1c 0500  ........W.......
000002f0: 2101 0001 8406 0000 2000 3010 91ff ffff  !....... .0.....
00000300: 1500 2c00 0000 0000 0060 0000 0000 0000  ..,......`......
00000310: 980b 0000 980b 0000 889e 0200 5701 0001  ............W...
00000320: 8406 0000 2000 3010 91ff ffff 8829 e86a  .... .0......).j
00000330: 91ff ffff 0200 0000 0000 0000 8838 0100  .............8..
00000340: 1b01 0001 8406 0000 2000 3010 91ff ffff  ........ .0.....
00000350: 1500 2c00 0000 0000 a029 e86a 91ff ffff  ..,......).j....
00000360: 2ab8 1800 3501 0001 8406 0000 2000 3010  *...5....... .0.
00000370: feff ffff 1500 2c00 0000 0000 0800 2c00  ......,.......,.
00000380: 0000 0000 0000 0000 2000 3010 32fe 0300  ........ .0.2...
00000390: 2201 0001 8406 0000 2000 3010 0000 0000  "....... .0.....
000003a0: 1500 2c00 0000 0000 ffff ffff ffff ff7f  ..,.............
000003b0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
000003c0: ffff ffff ffff ff7f 0000 0000 0000 0000  ................
000003d0: 0100 0000 0000 0000 887e 0800 5701 0001  .........~..W...
000003e0: 8406 0000 2000 3010 7b04 3200 7c3f e86a  .... .0.{.2.|?.j
000003f0: 91ff ffff 0a00 0000 0000 0000 ec2d 0300  .............-..
00000400: 2301 0001 8406 0000 2000 3010 7b04 3200  #....... .0.{.2.
00000410: 1500 2c00 0000 0000 0000 0000 0000 0000  ..,.............
00000420: ffff ffff ffff ff7f 0100 0000 3c01 0001  ............<...
00000430: 0a42 0c00 2401 0001 8406 0000 2000 3010  .B..$....... .0.
00000440: 0800 0000 1500 2c00 0000 0000 0000 0000  ......,.........
00000450: 0000 0000 0700 0000 0002 0000 885f 0100  ............._..
00000460: 6c01 0001 8406 0000 2000 3010 0100 0000  l....... .0.....
00000470: 1500 2c00 0000 0000 0000 0000 566b 0000  ..,.........Vk..
00000480: 0c25 0100 6d01 0001 8406 0000 2000 3010  .%..m....... .0.
00000490: 0400 0000 1500 2c00 0000 0000 0000 0000  ......,.........
000004a0: 0700 0000 ffff ffff ffff ff07 1400 0000  ................
000004b0: 0100 0000 caee 0100 5101 0001 8406 0000  ........Q.......
000004c0: 2000 3010 1100 0000 1500 2c00 0000 0000   .0.......,.....
000004d0: 0000 0000 0700 0000 2500 0000 2000 3010  ........%... .0.
000004e0: 323b 0600 3201 0001 8406 0000 2000 3010  2;..2....... .0.
000004f0: c86e 0000 1500 2c00 0000 0000 0700 0000  .n....,.........
00000500: 0000 0000 0000 0000 0000 0000 0080 b000  ................
00000510: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000520: 0000 0000 2024 0000 0400 0000 ae0a 1a00  .... $..........
00000530: 3a01 0001 8406 0000 2000 3010 0000 0000  :....... .0.....
00000540: 1500 2c00 0000 0000 0000 0000 0000 0000  ..,.............
00000550: 6101 0000 0700 0000 0000 0000 cf63 0000  a............c..
00000560: 4100 0000 0700 0000 b4c5 0200 3301 0001  A...........3...
00000570: 8406 0000 2000 3010 2000 3010 1500 2c00  .... .0. .0...,.
00000580: 0000 0000 cfe3 2000 0000 0000 0700 0000  ...... .........
00000590: 0000 0000 0000 0000 0000 0000 0080 b000  ................
000005a0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
000005b0: 0000 0000 2024 0000 6c01 0001 4859 0400  .... $..l...HY..
000005c0: 1b01 0001 8406 0000 2000 3010 0000 0000  ........ .0.....
000005d0: 1500 2c00 0000 0000 9c99 ea6a 91ff ffff  ..,........j....
000005e0: c850 0500 6001 0001 8406 0000 2000 3010  .P..`....... .0.
000005f0: 0000 0000 1500 2c00 0000 0000 0000 0000  ......,.........
00000600: 0700 0000 2ee6 0200 3e01 0002 8406 0000  ........>.......
00000610: 2000 3010 566b 0000 1500 2c00 0000 0000   .0.Vk....,.....
00000620: 0800 0000 0000 0000 0700 0000 0700 0000  ................
00000630: 0000 0000 0000 0000 0100 0000 8081 3010  ..............0.
00000640: a804 0400 5701 0001 8406 0000 2000 3010  ....W....... .0.
00000650: cb07 3200 885a ea6a 91ff ffff 0100 0000  ..2..Z.j........
00000660: 0000 0000 8875 0300 6c01 0001 8406 0000  .....u..l.......
00000670: 2000 3010 0300 0000 0300 0000 0000 0000   .0.............
00000680: 0100 0000 0100 0000 ccd4 0100 6d01 0001  ............m...
00000690: 8406 0000 2000 3010 cb07 3200 0300 0000  .... .0...2.....
000006a0: 0000 0000 0000 0000 0200 0000 2724 0000  ............'$..
000006b0: 0000 0000 1100 3010 0100 0000 a850 0500  ......0......P..
000006c0: 5701 0001 8406 0000 2000 3010 0000 0000  W....... .0.....
000006d0: 885a ea6a 91ff ffff 0100 0000 0000 0000  .Z.j............
000006e0: 680f 0200 6c01 0001 8406 0000 2000 3010  h...l....... .0.
000006f0: 0000 0000 0400 0000 0000 0000 0100 0000  ................
00000700: 6d01 0001 ac79 0100 6d01 0001 8406 0000  m....y..m.......
00000710: 2000 3010 0000 0000 0400 0000 0000 0000   .0.............
00000720: 0000 0000 0200 0000 2924 0000 0000 0000  ........)$......
00000730: 1143 0200 0100 0000 2818 0400 5701 0001  .C......(...W...
00000740: 8406 0000 2000 3010 0000 0000 8829 e86a  .... .0......).j
00000750: 91ff ffff 0200 0000 0000 0000 8838 0100  .............8..
00000760: 1b01 0001 8406 0000 2000 3010 0400 0000  ........ .0.....
00000770: 1500 2c00 0000 0000 a029 e86a 91ff ffff  ..,......).j....
00000780: 0e89 0300 5301 0001 8406 0000 2000 3010  ....S....... .0.
00000790: e128 0000 1500 2c00 0000 0000 2500 0000  .(....,.....%...
000007a0: 0000 0000 cfe3 2000 0000 0000 0000 0000  ...... .........
000007b0: 0700 0000 6000 0000 0700 0000 aca0 0100  ....`...........
000007c0: 6701 0001 8406 0000 2000 3010 e128 0000  g....... .0..(..
000007d0: 1500 2c00 0000 0000 0000 0000 0700 0000  ..,.............
000007e0: cfe3 2000 0000 0000 01a2 0800 0000 0000  .. .............
000007f0: 28b2 1e00 1b01 0001 8406 0000 2000 3010  (........... .0.
00000800: e128 0000 1500 2c00 0000 0000 9846 e86a  .(....,......F.j
00000810: 91ff ffff 68d2 1b00 5701 0001 8406 0000  ....h...W.......
00000820: 2000 3010 e128 0000 7c3f e86a 91ff ffff   .0..(..|?.j....
00000830: 0a00 0000 0000 0000 0c57 0200 2301 0001  .........W..#...
00000840: 8406 0000 2000 3010 006c 0000 1500 2c00  .... .0..l....,.
00000850: 0000 0000 0700 0000 0000 0000 f8ff ffff  ................
00000860: ffff ff7f 0100 0000 0000 0000 6e69 8200  ............ni..
00000870: 2501 0001 8406 0000 2000 3010 ca6e 0000  %....... .0..n..
00000880: 1500 2c00 0000 0000 0000 0000 0700 0000  ..,.............
00000890: 0000 0000 0000 0000 0700 0000 0000 0000  ................
000008a0: 0100 0000 0200 3010 3e13 bd82 5500 0000  ......0.>...U...
000008b0: 0600 0000 3001 0001 8015 0000 2000 3010  ....0....... .0.
000008c0: 0000 0000 c801 1400 0000 0000 8860 4404  .............`D.
000008d0: 1c01 0001 8015 0000 2000 3010 2000 0000  ........ .0. ...
000008e0: c801 1400 0000 0000 0000 0000 0000 0000  ................
000008f0: 88a9 0300 5701 0001 8015 0000 2000 3010  ....W....... .0.
00000900: 0400 0000 1c1e e86a 91ff ffff 0300 0000  .......j........
00000910: 0000 0000 a85a 1600 1b01 0001 8015 0000  .....Z..........
00000920: 2000 3010 2000 3010 c801 1400 0000 0000   .0. .0.........
00000930: c41e e86a 91ff ffff ca95 1c00 2901 0001  ...j........)...
00000940: 8015 0000 2000 3010 2000 3010 c801 1400  .... .0. .0.....
00000950: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000960: 0010 0000 c8fb 0100 2801 0001 8015 0000  ........(.......
00000970: 2000 3010 5101 0001 c801 1400 0000 0000   .0.Q...........
00000980: 0000 0000 0000 0000 6af1 0600 2901 0001  ........j...)...
00000990: 8015 0000 2000 3010 0000 0000 c801 1400  .... .0.........
000009a0: 0000 0000 0100 0000 0000 0000 0000 0000  ................
000009b0: 0010 0000 488f 0000 2801 0001 8015 0000  ....H...(.......
000009c0: 2000 3010 0200 ffff c801 1400 0000 0000   .0.............
000009d0: 0100 0000 0000 0000 483b 0900 4d01 0001  ........H;..M...
000009e0: 8015 0000 2000 3010 0000 0000 c801 1400  .... .0.........
000009f0: 0000 0000 1800 0000 0000 0000 8852 0100  .............R..
00000a00: 5701 0001 8015 0000 2000 3010 e128 0000  W....... .0..(..
00000a10: 9ce9 e76a 91ff ffff 0a00 0000 0000 0000  ...j............
00000a20: e615 0200 3001 0001 8015 0000 2000 3010  ....0....... .0.
00000a30: 0155 0000 c801 1400 0000 0000 68d0 0000  .U..........h...
00000a40: 1b01 0001 8015 0000 2000 3010 6606 3200  ........ .0.f.2.
00000a50: c801 1400 0000 0000 acfa ea6a 91ff ffff  ...........j....
00000a60: 6a0f 0200 6901 0001 8015 0000 2000 3010  j...i....... .0.
00000a70: 7106 3200 c801 1400 0000 0000 0000 0000  q.2.............
00000a80: 0000 0000 ffff ffff 0000 0000 e895 0300  ................
00000a90: 5701 0001 8015 0000 2000 3010 0300 0000  W....... .0.....
00000aa0: acbe ea6a 91ff ffff 0100 0000 0000 0000  ...j............
00000ab0: 8a38 0100 6501 0001 8015 0000 2000 3010  .8..e....... .0.
00000ac0: c41e e86a c801 1400 0000 0000 0000 0000  ...j............
00000ad0: feff ffff 0000 0000 0000 0000 ee86 0400  ................
00000ae0: 6301 0001 8015 0000 2000 3010 0000 0000  c....... .0.....
00000af0: c801 1400 0000 0000 0000 0000 0000 0000  ................
00000b00: 0000 0000 0000 0000 4285 5000 0000 0000  ........B.P.....
00000b10: 0200 0000 0000 0000 8e36 0200 6201 0001  .........6..b...
00000b20: 8015 0000 2000 3010 7d55 0000 c801 1400  .... .0.}U......
00000b30: 0000 0000 0000 0000 0100 0000 0000 0000  ................
00000b40: 0000 0000 4285 5000 0000 0000 0000 0000  ....B.P.........
00000b50: 0200 3010 8c5f 0100 3401 0001 8015 0000  ..0.._..4.......
00000b60: 2000 3010 0000 0000 c801 1400 0000 0000   .0.............
00000b70: 4285 5000 0000 0000 0200 0000 0000 0000  B.P.............
00000b80: 1000 0000 8081 0000 aa43 0500 3c01 0001  .........C..<...
00000b90: 8015 0000 2000 3010 2801 0001 c801 1400  .... .0.(.......
00000ba0: 0000 0000 4205 0000 a100 0000 0200 0000  ....B...........
00000bb0: 0200 0000 8871 0800 5701 0001 8015 0000  .....q..W.......
00000bc0: 2000 3010 2000 3010 885a ea6a 91ff ffff   .0. .0..Z.j....
00000bd0: 0100 0000 0000 0000 4825 0400 6c01 0001  ........H%..l...
00000be0: 8015 0000 2000 3010 2801 0001 0300 0000  .... .0.(.......
00000bf0: 0000 0000 0400 0000 7106 3200 0c73 0100  ........q.2..s..
00000c00: 6d01 0001 8015 0000 2000 3010 2901 0001  m....... .0.)...
00000c10: 0300 0000 0000 0000 0400 0000 0100 0000  ................
00000c20: 2680 0800 0000 0000 1100 0000 0100 0000  &...............
00000c30: c845 0400 5701 0001 8015 0000 2000 3010  .E..W....... .0.
00000c40: 2000 3010 885a ea6a 91ff ffff 0100 0000   .0..Z.j........
00000c50: 0000 0000 e8c9 0000 6c01 0001 8015 0000  ........l.......
00000c60: 2000 3010 2000 3010 0400 0000 0000 0000   .0. .0.........
00000c70: 0800 0000 0500 0000 6cdd 0000 6d01 0001  ........l...m...
00000c80: 8015 0000 2000 3010 2801 0001 0400 0000  .... .0.(.......
00000c90: 0000 0000 0800 0000 0300 0000 1290 7000  ..............p.
00000ca0: 0000 0000 1100 0000 0100 0000 6875 0300  ............hu..
00000cb0: 5701 0001 8015 0000 2000 3010 7106 3200  W....... .0.q.2.
00000cc0: 8829 e86a 91ff ffff 0200 0000 0000 0000  .).j............
00000cd0: a847 0000 1b01 0001 8015 0000 2000 3010  .G.......... .0.
00000ce0: 9ce9 e76a c801 1400 0000 0000 a029 e86a  ...j.........).j
00000cf0: 91ff ffff e83a 0300 1b01 0001 8015 0000  .....:..........
00000d00: 2000 3010 7106 3200 c801 1400 0000 0000   .0.q.2.........
00000d10: 9c99 ea6a 91ff ffff ae93 0100 6601 0001  ...j........f...
00000d20: 8015 0000 2000 3010 acfa ea6a c801 1400  .... .0....j....
00000d30: 0000 0000 0000 0000 feff ffff 0000 0000  ................
00000d40: 2000 3010 0000 0000 0000 0000 0000 0000   .0.............
00000d50: 0000 0000 48b6 0000 1b01 0001 8015 0000  ....H...........
00000d60: 2000 3010 e128 0000 c801 1400 0000 0000   .0..(..........
00000d70: 9c99 ea6a 91ff ffff a8ea 0300 1b01 0001  ...j............
00000d80: 8015 0000 2000 3010 e128 0000 c801 1400  .... .0..(......
00000d90: 0000 0000 08eb e76a 91ff ffff 885f 0100  .......j....._..
00000da0: 4e01 0001 8015 0000 2000 3010 2efe 0300  N....... .0.....
00000db0: c801 1400 0000 0000 0800 0000 0000 0000  ................
00000dc0: e8bc 0000 5701 0001 8015 0000 2000 3010  ....W....... .0.
00000dd0: 0000 0000 8829 e86a 91ff ffff 0200 0000  .....).j........
00000de0: 0000 0000 c895 0000 1b01 0001 8015 0000  ................
00000df0: 2000 3010 2000 3010 c801 1400 0000 0000   .0. .0.........
00000e00: a029 e86a 91ff ffff cab2 0b00 1e01 0001  .).j............
00000e10: 8015 0000 2000 3010 0000 0000 c801 1400  .... .0.........
00000e20: 0000 0000 0000 0000 0000 0000 0400 0000  ................
00000e30: 0000 0000 689a 0400 5701 0001 8015 0000  ....h...W.......
00000e40: 2000 3010 0000 0000 586f e86a 91ff ffff   .0.....Xo.j....
00000e50: 0100 0000 0000 0000 8884 0200 6c01 0001  ............l...
00000e60: 8015 0000 2000 3010 8829 e86a c801 1400  .... .0..).j....
00000e70: 0000 0000 0000 0000 4100 0000 ac47 0000  ........A....G..
00000e80: 6d01 0001 8015 0000 2000 3010 e128 0000  m....... .0..(..
00000e90: c801 1400 0000 0000 0000 0000 0000 0000  ................
00000ea0: 0000 0000 0000 0000 001c 0200 0000 0000  ................
00000eb0: 2a66 0100 5101 0001 8015 0000 2000 3010  *f..Q....... .0.
00000ec0: 0000 0000 c801 1400 0000 0000 0000 0000  ................
00000ed0: 0100 0000 0000 0000 2000 3010 087e 0200  ........ .0..~..
00000ee0: 6a01 0001 8015 0000 2000 3010 0100 0000  j....... .0.....
00000ef0: c801 1400 0000 0000 0000 0000 0100 0000  ................
00000f00: 8c11 0100 6b01 0001 8015 0000 2000 3010  ....k....... .0.
00000f10: 1b01 0001 c801 1400 0000 0000 0000 0000  ................
00000f20: 0000 0000 0000 0000 0000 0000 0028 0000  .............(..
00000f30: 2000 3010 8c5f 0100 6701 0001 8015 0000   .0.._..g.......
00000f40: 2000 3010 0000 0000 c801 1400 0000 0000   .0.............
00000f50: 0000 0000 ffff ffff ffff ffff ffff ff07  ................
00000f60: 0800 0000 0700 0000 6e02 0200 5301 0001  ........n...S...
00000f70: 8015 0000 2000 3010 0100 0000 c801 1400  .... .0.........
00000f80: 0000 0000 0000 0000 2000 3010 0000 0000  ........ .0.....
00000f90: 0100 0000 0000 0000 0100 0000 0000 0000  ................
00000fa0: 0000 0000 cc3a 0300 3f01 0002 8015 0000  .....:..?.......
00000fb0: 2000 3010 7106 3200 c801 1400 0000 0000   .0.q.2.........
00000fc0: 0800 0000 0000 0000 0100 0000 0000 0000  ................
00000fd0: 8081 3010 3d00 0000 f542 0000 0000 0000  ..0.=....B......
00000fe0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000ff0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
    )",
};

TEST(CpuReaderTest, ParseExt4WithOverwrite) {
  const ExamplePage* test_case = &g_full_page_ext4;

  BundleProvider bundle_provider(base::kPageSize);
  ProtoTranslationTable* table = GetTable(test_case->name);
  auto page = PageFromXxd(test_case->data);

  FtraceDataSourceConfig ds_config = EmptyConfig();
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("sched", "sched_switch")));

  FtraceMetadata metadata{};
  CompactSchedBuffer compact_buffer;
  const uint8_t* parse_pos = page.get();
  base::Optional<CpuReader::PageHeader> page_header =
      CpuReader::ParsePageHeader(&parse_pos, table->page_header_size_len());

  const uint8_t* page_end = page.get() + base::kPageSize;
  ASSERT_TRUE(page_header.has_value());
  EXPECT_TRUE(page_header->lost_events);  // data loss
  EXPECT_TRUE(parse_pos < page_end);
  EXPECT_TRUE(parse_pos + page_header->size < page_end);

  size_t evt_bytes = CpuReader::ParsePagePayload(
      parse_pos, &page_header.value(), table, &ds_config, &compact_buffer,
      bundle_provider.writer(), &metadata);

  EXPECT_LT(0u, evt_bytes);

  auto bundle = bundle_provider.ParseProto();
  ASSERT_TRUE(bundle);
}

}  // namespace perfetto
