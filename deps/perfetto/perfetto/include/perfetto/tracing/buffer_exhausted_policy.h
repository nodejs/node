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

#ifndef INCLUDE_PERFETTO_TRACING_BUFFER_EXHAUSTED_POLICY_H_
#define INCLUDE_PERFETTO_TRACING_BUFFER_EXHAUSTED_POLICY_H_

namespace perfetto {

// Determines how SharedMemoryArbiterImpl::GetNewChunk() behaves when no free
// chunks are available.
enum class BufferExhaustedPolicy {
  // SharedMemoryArbiterImpl::GetNewChunk() will stall if no free SMB chunk is
  // available and wait for the tracing service to free one. Note that this
  // requires that messages the arbiter sends to the tracing service (from any
  // TraceWriter thread) will be received by it, even if all TraceWriter threads
  // are stalled.
  kStall,

  // SharedMemoryArbiterImpl::GetNewChunk() will return an invalid chunk if no
  // free SMB chunk is available. In this case, the TraceWriter will fall back
  // to a garbage chunk and drop written data until acquiring a future chunk
  // succeeds again.
  kDrop,

  // TODO(eseckler): Switch to kDrop by default and change the Android code to
  // explicitly request kStall instead.
  kDefault = kStall
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_BUFFER_EXHAUSTED_POLICY_H_
