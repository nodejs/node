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

#include "src/tracing/core/null_trace_writer.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

#include "perfetto/protozero/message.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

NullTraceWriter::NullTraceWriter()
    : delegate_(base::kPageSize), stream_(&delegate_) {
  cur_packet_.reset(new protos::pbzero::TracePacket());
  cur_packet_->Finalize();  // To avoid the DCHECK in NewTracePacket().
}

NullTraceWriter::~NullTraceWriter() {}

void NullTraceWriter::Flush(std::function<void()> callback) {
  // Flush() cannot be called in the middle of a TracePacket.
  PERFETTO_CHECK(cur_packet_->is_finalized());

  if (callback)
    callback();
}

NullTraceWriter::TracePacketHandle NullTraceWriter::NewTracePacket() {
  // If we hit this, the caller is calling NewTracePacket() without having
  // finalized the previous packet.
  PERFETTO_DCHECK(cur_packet_->is_finalized());
  cur_packet_->Reset(&stream_);
  return TraceWriter::TracePacketHandle(cur_packet_.get());
}

WriterID NullTraceWriter::writer_id() const {
  return 0;
}

uint64_t NullTraceWriter::written() const {
  return 0;
}

}  // namespace perfetto
