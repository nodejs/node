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

#ifndef INCLUDE_PERFETTO_PUBLIC_CONSUMER_API_H_
#define INCLUDE_PERFETTO_PUBLIC_CONSUMER_API_H_

#include <stddef.h>
#include <stdint.h>

// Public API for perfetto consumer, exposed to the rest of the Android tree.

namespace perfetto {
namespace consumer {

// State diagram (without error states):
// +-------------+
// | kConnecting |----------------------------+
// +-------------+                            | Create(deferred_start:true)
//        |  Create(deferred_start:false)     V
//        |                            +-------------+
//        |                            | kConfigured |
//        |                            +-------------+
//        |                                   |
//        V                                   | StartTracing()
//   +----------+                             |
//   | kTracing |<----------------------------+
//   +----------+
//        |
//        | [after TraceConfig.duration_ms]
//        V
// +-------------+
// | kTraceEnded |
// +-------------+
enum class State {
  // The trace session failed (e.g. the trace config is malformed or mmap
  // failed). Look at logcat -s perfetto to find out more.
  // This state is final and unrecoverable. The sessions needs to be destroyed
  // and recreated if this error happens.
  kTraceFailed = -3,

  // Failed to connect to the traced daemon.
  // This state is final and unrecoverable. The sessions needs to be destroyed
  // and recreated if this error happens.
  kConnectionError = -2,

  // Not really a state. This is only returned when an invalid handle is passed
  // to PollState().
  kSessionNotFound = -1,

  // Idle state.
  // This state is transitional an internal-only. Client will never see it.
  kIdle = 0,

  // Establishing the connection to the traced daemon.
  // This state is transitional. It is set soon after the Create() call and
  // transitions automatically to:
  // - kConfigured, if |deferred_start| == true in the trace config.
  // - kTracing, if |deferred_start| == false.
  // - An error state, e.g. if cannot reach the traced daemon.
  kConnecting = 1,

  // Tracing configured (buffers allocated) but not started.
  // This state is reached only when setting |deferred_start| == true and is
  // held until the client calls StartTracing().
  kConfigured = 2,

  // Tracing is active.
  // This state lasts for the whole duration of the trace session (i.e.
  // |duration_ms| in the trace config), after which the session transitions
  // either to the kTraceEnded state (if successful) or an error state.
  kTracing = 3,

  // Tracing ended succesfully. The trace buffer can now be retrieved through
  // the ReadTrace() call.
  // This state is final.
  kTraceEnded = 4,
};

using Handle = int64_t;
constexpr Handle kInvalidHandle = -1;

// Signature for callback function provided by the embedder to get notified
// about state changes.
using OnStateChangedCb = void (*)(Handle, State, void* /*callback_arg*/);

// None of the calls below are blocking, unless otherwise specified.

// Enables tracing with the given TraceConfig. If the trace config has the
// |deferred_start| flag set (see trace_config.proto) tracing is initialized
// but not started. An explicit call to StartTracing() must be issued in order
// to start the capture.
// Args:
//   [trace_config_proto, trace_config_len] point to a binary-encoded proto
//     containing the trace config. See //external/perfetto/docs/trace-config.md
//     for more details.
//   callback: a user-defined callback that will be invoked upon state changes.
//     The callback will be invoked on an internal thread and must not block.
//   callback_arg: an optional user-define argument that will be passed back to
//     all callback invocations.
// Return value:
//    Returns a handle that can be used to retrieve the trace or kInvalidHandle
//    in case of failure (e.g., the trace config is malformed).
Handle Create(const void* config_proto,
              size_t config_len,
              OnStateChangedCb callback,
              void* callback_arg);

// Starts recording the trace. Can be used only when setting the
// |deferred_start| flag in the trace config passed to Create().
// If the session is in the kConfigured state it transitions it to the kTracing
// state, starting the trace. In any other state, instead, it does nothing other
// than logging an error message. Hence, this method can be called only once.
// The estimated end-to-end (this call to ftrace enabling) latency is 2-3 ms
// on a Pixel 2.
// TODO(primiano): relax this and allow to recycle handles without
// re-configuring the trace session.
void StartTracing(Handle);

struct TraceBuffer {
  char* begin;
  size_t size;
};

// Retrieves the whole trace buffer. It avoids extra copies by directly mmaping
// the tmp fd passed to the traced daemon.
// Return value:
//   If the trace is ended (state == kTraceEnded) returns a buffer containing
//   the whole trace. This buffer can be parsed directly with libprotobuf.
//   The buffer lifetime is tied to the tracing session and is valid until the
//   Destroy() call.
//   If called before the session reaches the kTraceEnded state, a null buffer
//   is returned.
TraceBuffer ReadTrace(Handle);

// Destroys all the resources associated to the tracing session (connection to
// traced and trace buffer). The handle must not be used after this point.
// It's safe to call this regardless of the handle's current state and validity.
void Destroy(Handle);

// Returns the state of the tracing session (for debugging).
// Return value:
//   Returns the state of the session, if the handle is valid, otherwise returns
//   kSessionNotFound.
State PollState(Handle);

}  // namespace consumer
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_PUBLIC_CONSUMER_API_H_
