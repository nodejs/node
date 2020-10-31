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

#ifndef INCLUDE_PERFETTO_TRACING_EVENT_CONTEXT_H_
#define INCLUDE_PERFETTO_TRACING_EVENT_CONTEXT_H_

#include "perfetto/protozero/message_handle.h"
#include "perfetto/tracing/internal/track_event_internal.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace internal {
class TrackEventInternal;
}

// Allows adding custom arguments into track events. Example:
//
//   TRACE_EVENT_BEGIN("category", "Title",
//                     [](perfetto::EventContext ctx) {
//                       auto* dbg = ctx.event()->add_debug_annotations();
//                       dbg->set_name("name");
//                       dbg->set_int_value(1234);
//                     });
//
class PERFETTO_EXPORT EventContext {
 public:
  EventContext(EventContext&&) = default;

  // For Chromium during the transition phase to the client library.
  // TODO(eseckler): Remove once Chromium has switched to client lib entirely.
  explicit EventContext(protos::pbzero::TrackEvent* event)
      : event_(event), incremental_state_(nullptr) {}

  ~EventContext();

  protos::pbzero::TrackEvent* event() const { return event_; }

 private:
  template <typename, size_t, typename, typename>
  friend class TrackEventInternedDataIndex;
  friend class internal::TrackEventInternal;

  using TracePacketHandle =
      ::protozero::MessageHandle<protos::pbzero::TracePacket>;

  EventContext(TracePacketHandle, internal::TrackEventIncrementalState*);
  EventContext(const EventContext&) = delete;

  TracePacketHandle trace_packet_;
  protos::pbzero::TrackEvent* event_;
  internal::TrackEventIncrementalState* incremental_state_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_EVENT_CONTEXT_H_
