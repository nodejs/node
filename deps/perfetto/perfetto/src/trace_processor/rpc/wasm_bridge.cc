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

#include <emscripten/emscripten.h>
#include <map>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/rpc/rpc.h"

namespace perfetto {
namespace trace_processor {

using RequestID = uint32_t;

// Reply(): replies to a RPC method invocation.
// Called asynchronously (i.e. in a separate task) by the C++ code inside the
// trace processor to return data for a RPC method call.
// The function is generic and thankfully we need just one for all methods
// because the output is always a protobuf buffer.
using ReplyFunction = void (*)(const char* /*proto_reply_data*/,
                               uint32_t /*len*/);

namespace {
Rpc* g_trace_processor_rpc;
ReplyFunction g_reply;

// The buffer used to pass the request arguments. The caller (JS) decides how
// big this buffer should be in the Initialize() call.
uint8_t* g_req_buf;

}  // namespace

// +---------------------------------------------------------------------------+
// | Exported functions called by the JS/TS running in the worker.             |
// +---------------------------------------------------------------------------+
extern "C" {

// Returns the address of the allocated request buffer.
uint8_t* EMSCRIPTEN_KEEPALIVE Initialize(ReplyFunction, uint32_t);
uint8_t* Initialize(ReplyFunction reply_function, uint32_t req_buffer_size) {
  g_trace_processor_rpc = new Rpc();
  g_reply = reply_function;
  g_req_buf = new uint8_t[req_buffer_size];
  return g_req_buf;
}

// Ingests trace data.
void EMSCRIPTEN_KEEPALIVE trace_processor_parse(uint32_t);
void trace_processor_parse(size_t size) {
  // TODO(primiano): Parse() makes a copy of the data, which is unfortunate.
  // Ideally there should be a way to take the Blob coming from JS and move it.
  // See https://github.com/WebAssembly/design/issues/1162.
  auto status = g_trace_processor_rpc->Parse(g_req_buf, size);
  if (status.ok()) {
    g_reply("", 0);
  } else {
    PERFETTO_FATAL("Fatal failure while parsing the trace: %s",
                   status.c_message());
  }
}

// We keep the same signature as other methods even though we don't take input
// arguments for simplicity.
void EMSCRIPTEN_KEEPALIVE trace_processor_notify_eof(uint32_t);
void trace_processor_notify_eof(uint32_t /* size, not used. */) {
  g_trace_processor_rpc->NotifyEndOfFile();
  g_reply("", 0);
}

void EMSCRIPTEN_KEEPALIVE trace_processor_raw_query(uint32_t);
void trace_processor_raw_query(uint32_t size) {
  std::vector<uint8_t> res = g_trace_processor_rpc->RawQuery(g_req_buf, size);
  g_reply(reinterpret_cast<const char*>(res.data()),
          static_cast<uint32_t>(res.size()));
}

void EMSCRIPTEN_KEEPALIVE trace_processor_compute_metric(uint32_t);
void trace_processor_compute_metric(uint32_t size) {
  std::vector<uint8_t> res =
      g_trace_processor_rpc->ComputeMetric(g_req_buf, size);
  g_reply(reinterpret_cast<const char*>(res.data()),
          static_cast<uint32_t>(res.size()));
}

}  // extern "C"

}  // namespace trace_processor
}  // namespace perfetto
