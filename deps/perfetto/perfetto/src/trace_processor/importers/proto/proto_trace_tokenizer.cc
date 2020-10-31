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

#include "src/trace_processor/importers/proto/proto_trace_tokenizer.h"

#include <string>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/proto_decoder.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/ftrace/ftrace_module.h"
#include "src/trace_processor/importers/gzip/gzip_utils.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/proto_incremental_state.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_sorter.h"

#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using protozero::proto_utils::MakeTagLengthDelimited;
using protozero::proto_utils::ParseVarInt;

namespace {

constexpr uint8_t kTracePacketTag =
    MakeTagLengthDelimited(protos::pbzero::Trace::kPacketFieldNumber);

TraceBlobView Decompress(GzipDecompressor* decompressor, TraceBlobView input) {
  PERFETTO_DCHECK(gzip::IsGzipSupported());

  uint8_t out[4096];

  std::vector<uint8_t> data;
  data.reserve(input.length());

  // Ensure that the decompressor is able to cope with a new stream of data.
  decompressor->Reset();
  decompressor->SetInput(input.data(), input.length());

  using ResultCode = GzipDecompressor::ResultCode;
  for (auto ret = ResultCode::kOk; ret != ResultCode::kEof;) {
    auto res = decompressor->Decompress(out, base::ArraySize(out));
    ret = res.ret;
    if (ret == ResultCode::kError || ret == ResultCode::kNoProgress ||
        ret == ResultCode::kNeedsMoreInput)
      return TraceBlobView(nullptr, 0, 0);

    data.insert(data.end(), out, out + res.bytes_written);
  }

  std::unique_ptr<uint8_t[]> output(new uint8_t[data.size()]);
  memcpy(output.get(), data.data(), data.size());
  return TraceBlobView(std::move(output), 0, data.size());
}

}  // namespace

ProtoTraceTokenizer::ProtoTraceTokenizer(TraceProcessorContext* ctx)
    : context_(ctx) {}
ProtoTraceTokenizer::~ProtoTraceTokenizer() = default;

util::Status ProtoTraceTokenizer::Parse(std::unique_ptr<uint8_t[]> owned_buf,
                                        size_t size) {
  uint8_t* data = &owned_buf[0];
  if (!partial_buf_.empty()) {
    // It takes ~5 bytes for a proto preamble + the varint size.
    const size_t kHeaderBytes = 5;
    if (PERFETTO_UNLIKELY(partial_buf_.size() < kHeaderBytes)) {
      size_t missing_len = std::min(kHeaderBytes - partial_buf_.size(), size);
      partial_buf_.insert(partial_buf_.end(), &data[0], &data[missing_len]);
      if (partial_buf_.size() < kHeaderBytes)
        return util::OkStatus();
      data += missing_len;
      size -= missing_len;
    }

    // At this point we have enough data in |partial_buf_| to read at least the
    // field header and know the size of the next TracePacket.
    const uint8_t* pos = &partial_buf_[0];
    uint8_t proto_field_tag = *pos;
    uint64_t field_size = 0;
    const uint8_t* next = ParseVarInt(++pos, &*partial_buf_.end(), &field_size);
    bool parse_failed = next == pos;
    pos = next;
    if (proto_field_tag != kTracePacketTag || field_size == 0 || parse_failed) {
      return util::ErrStatus(
          "Failed parsing a TracePacket from the partial buffer");
    }

    // At this point we know how big the TracePacket is.
    size_t hdr_size = static_cast<size_t>(pos - &partial_buf_[0]);
    size_t size_incl_header = static_cast<size_t>(field_size + hdr_size);
    PERFETTO_DCHECK(size_incl_header > partial_buf_.size());

    // There is a good chance that between the |partial_buf_| and the new |data|
    // of the current call we have enough bytes to parse a TracePacket.
    if (partial_buf_.size() + size >= size_incl_header) {
      // Create a new buffer for the whole TracePacket and copy into that:
      // 1) The beginning of the TracePacket (including the proto header) from
      //    the partial buffer.
      // 2) The rest of the TracePacket from the current |data| buffer (note
      //    that we might have consumed already a few bytes form |data| earlier
      //    in this function, hence we need to keep |off| into account).
      std::unique_ptr<uint8_t[]> buf(new uint8_t[size_incl_header]);
      memcpy(&buf[0], partial_buf_.data(), partial_buf_.size());
      // |size_missing| is the number of bytes for the rest of the TracePacket
      // in |data|.
      size_t size_missing = size_incl_header - partial_buf_.size();
      memcpy(&buf[partial_buf_.size()], &data[0], size_missing);
      data += size_missing;
      size -= size_missing;
      partial_buf_.clear();
      uint8_t* buf_start = &buf[0];  // Note that buf is std::moved below.
      util::Status status =
          ParseInternal(std::move(buf), buf_start, size_incl_header);
      if (PERFETTO_UNLIKELY(!status.ok()))
        return status;
    } else {
      partial_buf_.insert(partial_buf_.end(), data, &data[size]);
      return util::OkStatus();
    }
  }
  return ParseInternal(std::move(owned_buf), data, size);
}

util::Status ProtoTraceTokenizer::ParseInternal(
    std::unique_ptr<uint8_t[]> owned_buf,
    uint8_t* data,
    size_t size) {
  PERFETTO_DCHECK(data >= &owned_buf[0]);
  const uint8_t* start = &owned_buf[0];
  const size_t data_off = static_cast<size_t>(data - start);
  TraceBlobView whole_buf(std::move(owned_buf), data_off, size);

  protos::pbzero::Trace::Decoder decoder(data, size);
  for (auto it = decoder.packet(); it; ++it) {
    protozero::ConstBytes packet = *it;
    size_t field_offset = whole_buf.offset_of(packet.data);
    util::Status status =
        ParsePacket(whole_buf.slice(field_offset, packet.size));
    if (PERFETTO_UNLIKELY(!status.ok()))
      return status;
  }

  const size_t bytes_left = decoder.bytes_left();
  if (bytes_left > 0) {
    PERFETTO_DCHECK(partial_buf_.empty());
    partial_buf_.insert(partial_buf_.end(), &data[decoder.read_offset()],
                        &data[decoder.read_offset() + bytes_left]);
  }
  return util::OkStatus();
}

util::Status ProtoTraceTokenizer::ParsePacket(TraceBlobView packet) {
  protos::pbzero::TracePacket::Decoder decoder(packet.data(), packet.length());
  if (PERFETTO_UNLIKELY(decoder.bytes_left()))
    return util::ErrStatus(
        "Failed to parse proto packet fully; the trace is probably corrupt.");

  const uint32_t seq_id = decoder.trusted_packet_sequence_id();
  auto* state = GetIncrementalStateForPacketSequence(seq_id);

  uint32_t sequence_flags = decoder.sequence_flags();

  if (decoder.incremental_state_cleared() ||
      sequence_flags &
          protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED) {
    HandleIncrementalStateCleared(decoder);
  } else if (decoder.previous_packet_dropped()) {
    HandlePreviousPacketDropped(decoder);
  }

  // It is important that we parse defaults before parsing other fields such as
  // the timestamp, since the defaults could affect them.
  if (decoder.has_trace_packet_defaults()) {
    auto field = decoder.trace_packet_defaults();
    const size_t offset = packet.offset_of(field.data);
    ParseTracePacketDefaults(decoder, packet.slice(offset, field.size));
  }

  if (decoder.has_interned_data()) {
    auto field = decoder.interned_data();
    const size_t offset = packet.offset_of(field.data);
    ParseInternedData(decoder, packet.slice(offset, field.size));
  }

  if (decoder.has_clock_snapshot()) {
    return ParseClockSnapshot(decoder.clock_snapshot(),
                              decoder.trusted_packet_sequence_id());
  }

  if (decoder.sequence_flags() &
      protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE) {
    if (!seq_id) {
      return util::ErrStatus(
          "TracePacket specified SEQ_NEEDS_INCREMENTAL_STATE but the "
          "TraceWriter's sequence_id is zero (the service is "
          "probably too old)");
    }

    if (!state->IsIncrementalStateValid()) {
      context_->storage->IncrementStats(stats::tokenizer_skipped_packets);
      return util::OkStatus();
    }
  }

  protos::pbzero::TracePacketDefaults::Decoder* defaults =
      state->current_generation()->GetTracePacketDefaults();

  int64_t timestamp;
  if (decoder.has_timestamp()) {
    timestamp = static_cast<int64_t>(decoder.timestamp());

    uint32_t timestamp_clock_id =
        decoder.has_timestamp_clock_id()
            ? decoder.timestamp_clock_id()
            : (defaults ? defaults->timestamp_clock_id() : 0);

    if ((decoder.has_chrome_events() || decoder.has_chrome_metadata()) &&
        (!timestamp_clock_id ||
         timestamp_clock_id ==
             protos::pbzero::ClockSnapshot::Clock::MONOTONIC)) {
      // Chrome event timestamps are in MONOTONIC domain, but may occur in
      // traces where (a) no clock snapshots exist or (b) no clock_id is
      // specified for their timestamps. Adjust to trace time if we have a clock
      // snapshot.
      // TODO(eseckler): Set timestamp_clock_id and emit ClockSnapshots in
      // chrome and then remove this.
      auto trace_ts = context_->clock_tracker->ToTraceTime(
          protos::pbzero::ClockSnapshot::Clock::MONOTONIC, timestamp);
      if (trace_ts.has_value())
        timestamp = trace_ts.value();
    } else if (timestamp_clock_id) {
      // If the TracePacket specifies a non-zero clock-id, translate the
      // timestamp into the trace-time clock domain.
      ClockTracker::ClockId converted_clock_id = timestamp_clock_id;
      bool is_seq_scoped =
          ClockTracker::IsReservedSeqScopedClockId(converted_clock_id);
      if (is_seq_scoped) {
        if (!seq_id) {
          return util::ErrStatus(
              "TracePacket specified a sequence-local clock id (%" PRIu32
              ") but the TraceWriter's sequence_id is zero (the service is "
              "probably too old)",
              timestamp_clock_id);
        }
        converted_clock_id =
            ClockTracker::SeqScopedClockIdToGlobal(seq_id, timestamp_clock_id);
      }
      auto trace_ts =
          context_->clock_tracker->ToTraceTime(converted_clock_id, timestamp);
      if (!trace_ts.has_value()) {
        // ToTraceTime() will increase the |clock_sync_failure| stat on failure.
        static const char seq_extra_err[] =
            " Because the clock id is sequence-scoped, the ClockSnapshot must "
            "be emitted on the same TraceWriter sequence of the packet that "
            "refers to that clock id.";
        return util::ErrStatus(
            "Failed to convert TracePacket's timestamp from clock_id=%" PRIu32
            " seq_id=%" PRIu32
            ". This is usually due to the lack of a prior ClockSnapshot "
            "proto.%s",
            timestamp_clock_id, seq_id, is_seq_scoped ? seq_extra_err : "");
      }
      timestamp = trace_ts.value();
    }
  } else {
    timestamp = std::max(latest_timestamp_, context_->sorter->max_timestamp());
  }
  latest_timestamp_ = std::max(timestamp, latest_timestamp_);

  auto& modules = context_->modules_by_field;
  for (uint32_t field_id = 1; field_id < modules.size(); ++field_id) {
    if (modules[field_id] && decoder.Get(field_id).valid()) {
      ModuleResult res = modules[field_id]->TokenizePacket(
          decoder, &packet, timestamp, state, field_id);
      if (!res.ignored())
        return res.ToStatus();
    }
  }

  if (decoder.has_compressed_packets()) {
    if (!gzip::IsGzipSupported())
      return util::Status("Cannot decode compressed packets. Zlib not enabled");

    protozero::ConstBytes field = decoder.compressed_packets();
    const size_t field_off = packet.offset_of(field.data);
    TraceBlobView compressed_packets = packet.slice(field_off, field.size);
    TraceBlobView packets =
        Decompress(&decompressor_, std::move(compressed_packets));

    const uint8_t* start = packets.data();
    const uint8_t* end = packets.data() + packets.length();
    const uint8_t* ptr = start;
    while ((end - ptr) > 2) {
      const uint8_t* packet_start = ptr;
      if (PERFETTO_UNLIKELY(*ptr != kTracePacketTag))
        return util::ErrStatus("Expected TracePacket tag");
      uint64_t packet_size = 0;
      ptr = ParseVarInt(++ptr, end, &packet_size);
      size_t packet_offset = static_cast<size_t>(ptr - start);
      ptr += packet_size;
      if (PERFETTO_UNLIKELY((ptr - packet_start) < 2 || ptr > end))
        return util::ErrStatus("Invalid packet size");
      util::Status status = ParsePacket(
          packets.slice(packet_offset, static_cast<size_t>(packet_size)));
      if (PERFETTO_UNLIKELY(!status.ok()))
        return status;
    }
    return util::OkStatus();
  }

  // If we're not forcing a full sort and this is a write_into_file trace, then
  // use flush_period_ms as an indiciator for how big the sliding window for the
  // sorter should be.
  if (!context_->config.force_full_sort && decoder.has_trace_config()) {
    auto config = decoder.trace_config();
    protos::pbzero::TraceConfig::Decoder trace_config(config.data, config.size);

    if (trace_config.write_into_file()) {
      int64_t window_size_ns;
      if (trace_config.has_flush_period_ms() &&
          trace_config.flush_period_ms() > 0) {
        // We use 2x the flush period as a margin of error to allow for any
        // late flush responses to still be sorted correctly.
        window_size_ns = static_cast<int64_t>(trace_config.flush_period_ms()) *
                         2 * 1000 * 1000;
      } else {
        constexpr uint64_t kDefaultWindowNs =
            180 * 1000 * 1000 * 1000ULL;  // 3 minutes.
        PERFETTO_ELOG(
            "It is strongly recommended to have flush_period_ms set when "
            "write_into_file is turned on. You will likely have many dropped "
            "events because of inability to sort the events correctly.");
        window_size_ns = static_cast<int64_t>(kDefaultWindowNs);
      }
      context_->sorter->SetWindowSizeNs(window_size_ns);
    }
  }

  // Use parent data and length because we want to parse this again
  // later to get the exact type of the packet.
  context_->sorter->PushTracePacket(timestamp, state, std::move(packet));

  return util::OkStatus();
}

void ProtoTraceTokenizer::HandleIncrementalStateCleared(
    const protos::pbzero::TracePacket::Decoder& packet_decoder) {
  if (PERFETTO_UNLIKELY(!packet_decoder.has_trusted_packet_sequence_id())) {
    PERFETTO_ELOG(
        "incremental_state_cleared without trusted_packet_sequence_id");
    context_->storage->IncrementStats(stats::interned_data_tokenizer_errors);
    return;
  }
  GetIncrementalStateForPacketSequence(
      packet_decoder.trusted_packet_sequence_id())
      ->OnIncrementalStateCleared();
  context_->track_tracker->OnIncrementalStateCleared(
      packet_decoder.trusted_packet_sequence_id());
}

void ProtoTraceTokenizer::HandlePreviousPacketDropped(
    const protos::pbzero::TracePacket::Decoder& packet_decoder) {
  if (PERFETTO_UNLIKELY(!packet_decoder.has_trusted_packet_sequence_id())) {
    PERFETTO_ELOG("previous_packet_dropped without trusted_packet_sequence_id");
    context_->storage->IncrementStats(stats::interned_data_tokenizer_errors);
    return;
  }
  GetIncrementalStateForPacketSequence(
      packet_decoder.trusted_packet_sequence_id())
      ->OnPacketLoss();
}

void ProtoTraceTokenizer::ParseTracePacketDefaults(
    const protos::pbzero::TracePacket_Decoder& packet_decoder,
    TraceBlobView trace_packet_defaults) {
  if (PERFETTO_UNLIKELY(!packet_decoder.has_trusted_packet_sequence_id())) {
    PERFETTO_ELOG(
        "TracePacketDefaults packet without trusted_packet_sequence_id");
    context_->storage->IncrementStats(stats::interned_data_tokenizer_errors);
    return;
  }

  auto* state = GetIncrementalStateForPacketSequence(
      packet_decoder.trusted_packet_sequence_id());
  state->UpdateTracePacketDefaults(std::move(trace_packet_defaults));
}

void ProtoTraceTokenizer::ParseInternedData(
    const protos::pbzero::TracePacket::Decoder& packet_decoder,
    TraceBlobView interned_data) {
  if (PERFETTO_UNLIKELY(!packet_decoder.has_trusted_packet_sequence_id())) {
    PERFETTO_ELOG("InternedData packet without trusted_packet_sequence_id");
    context_->storage->IncrementStats(stats::interned_data_tokenizer_errors);
    return;
  }

  auto* state = GetIncrementalStateForPacketSequence(
      packet_decoder.trusted_packet_sequence_id());

  // Don't parse interned data entries until incremental state is valid, because
  // they could otherwise be associated with the wrong generation in the state.
  if (!state->IsIncrementalStateValid()) {
    context_->storage->IncrementStats(stats::tokenizer_skipped_packets);
    return;
  }

  // Store references to interned data submessages into the sequence's state.
  protozero::ProtoDecoder decoder(interned_data.data(), interned_data.length());
  for (protozero::Field f = decoder.ReadField(); f.valid();
       f = decoder.ReadField()) {
    auto bytes = f.as_bytes();
    auto offset = interned_data.offset_of(bytes.data);
    state->InternMessage(f.id(), interned_data.slice(offset, bytes.size));
  }
}

util::Status ProtoTraceTokenizer::ParseClockSnapshot(ConstBytes blob,
                                                     uint32_t seq_id) {
  std::vector<ClockTracker::ClockValue> clocks;
  protos::pbzero::ClockSnapshot::Decoder evt(blob.data, blob.size);
  for (auto it = evt.clocks(); it; ++it) {
    protos::pbzero::ClockSnapshot::Clock::Decoder clk(*it);
    ClockTracker::ClockId clock_id = clk.clock_id();
    if (ClockTracker::IsReservedSeqScopedClockId(clk.clock_id())) {
      if (!seq_id) {
        return util::ErrStatus(
            "ClockSnapshot packet is specifying a sequence-scoped clock id "
            "(%" PRIu64 ") but the TracePacket sequence_id is zero",
            clock_id);
      }
      clock_id = ClockTracker::SeqScopedClockIdToGlobal(seq_id, clk.clock_id());
    }
    int64_t unit_multiplier_ns =
        clk.unit_multiplier_ns()
            ? static_cast<int64_t>(clk.unit_multiplier_ns())
            : 1;
    clocks.emplace_back(clock_id, clk.timestamp(), unit_multiplier_ns,
                        clk.is_incremental());
  }
  context_->clock_tracker->AddSnapshot(clocks);
  return util::OkStatus();
}

void ProtoTraceTokenizer::NotifyEndOfFile() {}

}  // namespace trace_processor
}  // namespace perfetto
