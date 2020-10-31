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

#include "src/trace_processor/importers/proto/android_probes_module.h"

#include "perfetto/base/build_config.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/trace_processor/importers/proto/android_probes_parser.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_sorter.h"

#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/power/power_rails.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

AndroidProbesModule::AndroidProbesModule(TraceProcessorContext* context)
    : parser_(context), context_(context) {
  RegisterForField(TracePacket::kBatteryFieldNumber, context);
  RegisterForField(TracePacket::kPowerRailsFieldNumber, context);
  RegisterForField(TracePacket::kAndroidLogFieldNumber, context);
  RegisterForField(TracePacket::kPackagesListFieldNumber, context);
  RegisterForField(TracePacket::kInitialDisplayStateFieldNumber, context);
}

ModuleResult AndroidProbesModule::TokenizePacket(
    const protos::pbzero::TracePacket_Decoder&,
    TraceBlobView* packet,
    int64_t packet_timestamp,
    PacketSequenceState* state,
    uint32_t field_id) {
  if (field_id != TracePacket::kPowerRailsFieldNumber)
    return ModuleResult::Ignored();

  // Power rails are similar to ftrace in that they have many events, each with
  // their own timestamp, packed inside a single TracePacket. This means that,
  // similar to ftrace, we need to unpack them and individually sort them.

  // However, as these events are not perf sensitive, it's not worth adding
  // a lot of machinery to shepherd these events through the sorting queues
  // in a special way. Therefore, we just forge new packets and sort them as if
  // they came from the underlying trace.

  protos::pbzero::TracePacket::Decoder decoder(packet->data(),
                                               packet->length());
  auto power_rails = decoder.power_rails();
  protos::pbzero::PowerRails::Decoder evt(power_rails.data, power_rails.size);

  // Break out the rail descriptor into its own packet with the same timestamp
  // as the packet itself.
  if (evt.has_rail_descriptor()) {
    protozero::HeapBuffered<protos::pbzero::TracePacket> desc_packet;
    desc_packet->set_timestamp(static_cast<uint64_t>(packet_timestamp));

    auto* rails = desc_packet->set_power_rails();
    for (auto it = evt.rail_descriptor(); it; ++it) {
      protozero::ConstBytes desc = *it;
      rails->add_rail_descriptor()->AppendRawProtoBytes(desc.data, desc.size);
    }

    std::vector<uint8_t> vec = desc_packet.SerializeAsArray();
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[vec.size()]);
    memcpy(buffer.get(), vec.data(), vec.size());
    context_->sorter->PushTracePacket(
        packet_timestamp, state,
        TraceBlobView(std::move(buffer), 0, vec.size()));
  }

  // For each energy data message, turn it into its own trace packet
  // making sure its timestamp is consistent between the packet level and
  // the EnergyData level.
  for (auto it = evt.energy_data(); it; ++it) {
    protozero::ConstBytes bytes = *it;
    protos::pbzero::PowerRails_EnergyData_Decoder data(bytes.data, bytes.size);
    int64_t actual_ts =
        data.has_timestamp_ms()
            ? static_cast<int64_t>(data.timestamp_ms()) * 1000000
            : packet_timestamp;

    protozero::HeapBuffered<protos::pbzero::TracePacket> data_packet;
    data_packet->set_timestamp(static_cast<uint64_t>(actual_ts));

    auto* energy = data_packet->set_power_rails()->add_energy_data();
    energy->set_energy(data.energy());
    energy->set_index(data.index());
    energy->set_timestamp_ms(static_cast<uint64_t>(actual_ts / 1000000));

    std::vector<uint8_t> vec = data_packet.SerializeAsArray();
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[vec.size()]);
    memcpy(buffer.get(), vec.data(), vec.size());
    context_->sorter->PushTracePacket(
        actual_ts, state, TraceBlobView(std::move(buffer), 0, vec.size()));
  }

  return ModuleResult::Handled();
}

void AndroidProbesModule::ParsePacket(const TracePacket::Decoder& decoder,
                                      const TimestampedTracePiece& ttp,
                                      uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kBatteryFieldNumber:
      parser_.ParseBatteryCounters(ttp.timestamp, decoder.battery());
      return;
    case TracePacket::kPowerRailsFieldNumber:
      parser_.ParsePowerRails(ttp.timestamp, decoder.power_rails());
      return;
    case TracePacket::kAndroidLogFieldNumber:
      parser_.ParseAndroidLogPacket(decoder.android_log());
      return;
    case TracePacket::kPackagesListFieldNumber:
      parser_.ParseAndroidPackagesList(decoder.packages_list());
      return;
    case TracePacket::kInitialDisplayStateFieldNumber:
      parser_.ParseInitialDisplayState(ttp.timestamp,
                                       decoder.initial_display_state());
      return;
  }
}

void AndroidProbesModule::ParseTraceConfig(
    const protos::pbzero::TraceConfig::Decoder& decoder) {
  if (decoder.has_statsd_metadata()) {
    parser_.ParseStatsdMetadata(decoder.statsd_metadata());
  }
}

}  // namespace trace_processor
}  // namespace perfetto
