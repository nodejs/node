// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_GDB_REMOTE_UTIL_H_
#define V8_DEBUG_WASM_GDB_SERVER_GDB_REMOTE_UTIL_H_

#include <string>
#include <vector>

#include "src/utils/utils.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

#define TRACE_GDB_REMOTE(...)                                                \
  do {                                                                       \
    if (v8_flags.trace_wasm_gdb_remote) PrintF("[gdb-remote] " __VA_ARGS__); \
  } while (false)

// Convert from 0-255 to a pair of ASCII chars (0-9,a-f).
void UInt8ToHex(uint8_t byte, char chars[2]);

// Convert a pair of hex chars into a value 0-255 or return false if either
// input character is not a valid nibble.
bool HexToUInt8(const char chars[2], uint8_t* byte);

// Convert from ASCII (0-9,a-f,A-F) to 4b unsigned or return false if the
// input char is unexpected.
bool NibbleToUInt8(char ch, uint8_t* byte);

std::vector<std::string> V8_EXPORT_PRIVATE StringSplit(const std::string& instr,
                                                       const char* delim);

// Convert the memory pointed to by {mem} into a hex string in GDB-remote
// format.
std::string Mem2Hex(const uint8_t* mem, size_t count);
std::string Mem2Hex(const std::string& str);

// For LLDB debugging, an address in a Wasm module code space is represented
// with 64 bits, where the first 32 bits identify the module id:
// +--------------------+--------------------+
// |     module_id      |       offset       |
// +--------------------+--------------------+
//  <----- 32 bit -----> <----- 32 bit ----->
class wasm_addr_t {
 public:
  wasm_addr_t(uint32_t module_id, uint32_t offset)
      : module_id_(module_id), offset_(offset) {}
  explicit wasm_addr_t(uint64_t address)
      : module_id_(address >> 32), offset_(address & 0xffffffff) {}

  inline uint32_t ModuleId() const { return module_id_; }
  inline uint32_t Offset() const { return offset_; }

  inline operator uint64_t() const {
    return static_cast<uint64_t>(module_id_) << 32 | offset_;
  }

 private:
  uint32_t module_id_;
  uint32_t offset_;
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_GDB_REMOTE_UTIL_H_
