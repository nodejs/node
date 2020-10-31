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

#include "perfetto/tracing/track.h"

#include "perfetto/ext/base/uuid.h"
#include "perfetto/tracing/internal/track_event_data_source.h"
#include "protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"

namespace perfetto {

// static
uint64_t Track::process_uuid;

void Track::Serialize(protos::pbzero::TrackDescriptor* desc) const {
  desc->set_uuid(uuid);
  if (parent_uuid)
    desc->set_parent_uuid(parent_uuid);
}

void ProcessTrack::Serialize(protos::pbzero::TrackDescriptor* desc) const {
  Track::Serialize(desc);
  auto pd = desc->set_process();
  pd->set_pid(static_cast<int32_t>(pid));
  // TODO(skyostil): Record command line.
}

void ThreadTrack::Serialize(protos::pbzero::TrackDescriptor* desc) const {
  Track::Serialize(desc);
  auto td = desc->set_thread();
  td->set_pid(static_cast<int32_t>(pid));
  td->set_tid(static_cast<int32_t>(tid));
  // TODO(skyostil): Record thread name.
}

namespace internal {

// static
TrackRegistry* TrackRegistry::instance_;

TrackRegistry::TrackRegistry() = default;
TrackRegistry::~TrackRegistry() = default;

// static
void TrackRegistry::InitializeInstance() {
  // TODO(eseckler): Chrome may call this more than once. Once Chrome doesn't
  // call this directly anymore, bring back DCHECK(!instance_) instead.
  if (instance_)
    return;
  instance_ = new TrackRegistry();
  Track::process_uuid = static_cast<uint64_t>(base::Uuidv4().lsb());
}

void TrackRegistry::UpdateTrackImpl(
    Track track,
    std::function<void(protos::pbzero::TrackDescriptor*)> fill_function) {
  constexpr size_t kInitialSliceSize = 32;
  constexpr size_t kMaximumSliceSize = 4096;
  protozero::HeapBuffered<protos::pbzero::TrackDescriptor> new_descriptor(
      kInitialSliceSize, kMaximumSliceSize);
  fill_function(new_descriptor.get());
  auto serialized_desc = new_descriptor.SerializeAsString();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_[track.uuid] = std::move(serialized_desc);
  }
}

void TrackRegistry::EraseTrack(Track track) {
  std::lock_guard<std::mutex> lock(mutex_);
  tracks_.erase(track.uuid);
}

// static
void TrackRegistry::WriteTrackDescriptor(
    const SerializedTrackDescriptor& desc,
    protozero::MessageHandle<protos::pbzero::TracePacket> packet) {
  packet->AppendString(
      perfetto::protos::pbzero::TracePacket::kTrackDescriptorFieldNumber, desc);
}

}  // namespace internal
}  // namespace perfetto
