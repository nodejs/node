// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_UTIL_H_
#define V8_DEBUG_WASM_GDB_SERVER_UTIL_H_

#include <string>
#include "src/flags/flags.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

#define TRACE_GDB_REMOTE(...)                                            \
  do {                                                                   \
    if (FLAG_trace_wasm_gdb_remote) PrintF("[gdb-remote] " __VA_ARGS__); \
  } while (false)

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_UTIL_H_
