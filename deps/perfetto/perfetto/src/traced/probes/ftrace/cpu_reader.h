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

#ifndef SRC_TRACED_PROBES_FTRACE_CPU_READER_H_
#define SRC_TRACED_PROBES_FTRACE_CPU_READER_H_

#include <stdint.h>
#include <string.h>

#include <array>
#include <atomic>
#include <memory>
#include <set>
#include <thread>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/pipe.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/traced/data_source_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/protozero/message.h"
#include "perfetto/protozero/message_handle.h"
#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_metadata.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"

namespace perfetto {

class FtraceDataSource;
class ProtoTranslationTable;
struct FtraceDataSourceConfig;

namespace protos {
namespace pbzero {
class FtraceEventBundle;
}  // namespace pbzero
}  // namespace protos

// Reads raw ftrace data for a cpu, parses it, and writes it into the perfetto
// tracing buffers.
class CpuReader {
 public:
  using FtraceEventBundle = protos::pbzero::FtraceEventBundle;

  struct PageHeader {
    uint64_t timestamp;
    uint64_t size;
    bool lost_events;
  };

  CpuReader(size_t cpu,
            const ProtoTranslationTable* table,
            base::ScopedFile trace_fd);
  ~CpuReader();

  // Reads and parses all ftrace data for this cpu (in batches), until we catch
  // up to the writer, or hit |max_pages|. Returns number of pages read.
  size_t ReadCycle(uint8_t* parsing_buf,
                   size_t parsing_buf_size_pages,
                   size_t max_pages,
                   const std::set<FtraceDataSource*>& started_data_sources);

  template <typename T>
  static bool ReadAndAdvance(const uint8_t** ptr, const uint8_t* end, T* out) {
    if (*ptr > end - sizeof(T))
      return false;
    memcpy(reinterpret_cast<void*>(out), reinterpret_cast<const void*>(*ptr),
           sizeof(T));
    *ptr += sizeof(T);
    return true;
  }

  // Caller must do the bounds check:
  // [start + offset, start + offset + sizeof(T))
  // Returns the raw value not the varint.
  template <typename T>
  static T ReadIntoVarInt(const uint8_t* start,
                          uint32_t field_id,
                          protozero::Message* out) {
    T t;
    memcpy(&t, reinterpret_cast<const void*>(start), sizeof(T));
    out->AppendVarInt<T>(field_id, t);
    return t;
  }

  template <typename T>
  static void ReadInode(const uint8_t* start,
                        uint32_t field_id,
                        protozero::Message* out,
                        FtraceMetadata* metadata) {
    T t = ReadIntoVarInt<T>(start, field_id, out);
    metadata->AddInode(static_cast<Inode>(t));
  }

  template <typename T>
  static void ReadDevId(const uint8_t* start,
                        uint32_t field_id,
                        protozero::Message* out,
                        FtraceMetadata* metadata) {
    T t;
    memcpy(&t, reinterpret_cast<const void*>(start), sizeof(T));
    BlockDeviceID dev_id = TranslateBlockDeviceIDToUserspace<T>(t);
    out->AppendVarInt<BlockDeviceID>(field_id, dev_id);
    metadata->AddDevice(dev_id);
  }

  static void ReadPid(const uint8_t* start,
                      uint32_t field_id,
                      protozero::Message* out,
                      FtraceMetadata* metadata) {
    int32_t pid = ReadIntoVarInt<int32_t>(start, field_id, out);
    metadata->AddPid(pid);
  }

  static void ReadCommonPid(const uint8_t* start,
                            uint32_t field_id,
                            protozero::Message* out,
                            FtraceMetadata* metadata) {
    int32_t pid = ReadIntoVarInt<int32_t>(start, field_id, out);
    metadata->AddCommonPid(pid);
  }

  // Internally the kernel stores device ids in a different layout to that
  // exposed to userspace via stat etc. There's no userspace function to convert
  // between the formats so we have to do it ourselves.
  template <typename T>
  static BlockDeviceID TranslateBlockDeviceIDToUserspace(T kernel_dev) {
    // Provided search index s_dev from
    // https://github.com/torvalds/linux/blob/v4.12/include/linux/fs.h#L404
    // Convert to user space id using
    // https://github.com/torvalds/linux/blob/v4.12/include/linux/kdev_t.h#L10
    // TODO(azappone): see if this is the same on all platforms
    uint64_t maj = static_cast<uint64_t>(kernel_dev) >> 20;
    uint64_t min = static_cast<uint64_t>(kernel_dev) & ((1U << 20) - 1);
    return static_cast<BlockDeviceID>(  // From makedev()
        ((maj & 0xfffff000ULL) << 32) | ((maj & 0xfffULL) << 8) |
        ((min & 0xffffff00ULL) << 12) | ((min & 0xffULL)));
  }

  // Returns a parsed representation of the given raw ftrace page's header.
  static base::Optional<CpuReader::PageHeader> ParsePageHeader(
      const uint8_t** ptr,
      uint16_t page_header_size_len);

  // Parse the payload of a raw ftrace page, and write the events as protos
  // into the provided bundle (and/or compact buffer).
  // |table| contains the mix of compile time (e.g. proto field ids) and
  // run time (e.g. field offset and size) information necessary to do this.
  // The table is initialized once at start time by the ftrace controller
  // which passes it to the CpuReader which passes it here.
  // The caller is responsible for validating that the page_header->size stays
  // within the current page.
  static size_t ParsePagePayload(const uint8_t* start_of_payload,
                                 const PageHeader* page_header,
                                 const ProtoTranslationTable* table,
                                 const FtraceDataSourceConfig* ds_config,
                                 CompactSchedBuffer* compact_sched_buffer,
                                 FtraceEventBundle* bundle,
                                 FtraceMetadata* metadata);

  // Parse a single raw ftrace event beginning at |start| and ending at |end|
  // and write it into the provided bundle as a proto.
  // |table| contains the mix of compile time (e.g. proto field ids) and
  // run time (e.g. field offset and size) information necessary to do this.
  // The table is initialized once at start time by the ftrace controller
  // which passes it to the CpuReader which passes it to ParsePage which
  // passes it here.
  static bool ParseEvent(uint16_t ftrace_event_id,
                         const uint8_t* start,
                         const uint8_t* end,
                         const ProtoTranslationTable* table,
                         protozero::Message* message,
                         FtraceMetadata* metadata);

  static bool ParseField(const Field& field,
                         const uint8_t* start,
                         const uint8_t* end,
                         protozero::Message* message,
                         FtraceMetadata* metadata);

  // Parse a sched_switch event according to pre-validated format, and buffer
  // the individual fields in the given compact encoding batch.
  static void ParseSchedSwitchCompact(const uint8_t* start,
                                      uint64_t timestamp,
                                      const CompactSchedSwitchFormat* format,
                                      CompactSchedBuffer* compact_buf,
                                      FtraceMetadata* metadata);

  // Parse a sched_waking event according to pre-validated format, and buffer
  // the individual fields in the given compact encoding batch.
  static void ParseSchedWakingCompact(const uint8_t* start,
                                      uint64_t timestamp,
                                      const CompactSchedWakingFormat* format,
                                      CompactSchedBuffer* compact_buf,
                                      FtraceMetadata* metadata);

  // Parses & encodes the given range of contiguous tracing pages. Called by
  // |ReadAndProcessBatch| for each active data source.
  //
  // public and static for testing
  static bool ProcessPagesForDataSource(TraceWriter* trace_writer,
                                        FtraceMetadata* metadata,
                                        size_t cpu,
                                        const FtraceDataSourceConfig* ds_config,
                                        const uint8_t* parsing_buf,
                                        const size_t pages_read,
                                        const ProtoTranslationTable* table);

 private:
  CpuReader(const CpuReader&) = delete;
  CpuReader& operator=(const CpuReader&) = delete;

  // Reads at most |max_pages| of ftrace data, parses it, and writes it
  // into |started_data_sources|. Returns number of pages read.
  // See comment on ftrace_controller.cc:kMaxParsingWorkingSetPages for
  // rationale behind the batching.
  size_t ReadAndProcessBatch(
      uint8_t* parsing_buf,
      size_t max_pages,
      bool first_batch_in_cycle,
      const std::set<FtraceDataSource*>& started_data_sources);

  const size_t cpu_;
  const ProtoTranslationTable* const table_;
  base::ScopedFile trace_fd_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_CPU_READER_H_
