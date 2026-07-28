// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_GDB_REMOTE_UTIL_H_
#define V8_DEBUG_WASM_GDB_SERVER_GDB_REMOTE_UTIL_H_

#include <string>
#include <vector>

#include "src/flags/flags.h"
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

// For LLDB debugging, the separate 32-bit WebAssembly address spaces (code and
// linear memory) are encoded into a single 64-bit virtual address. This matches
// the encoding used by LLDB's own wasm_addr_t, so that code and memory occupy
// distinct address spaces (selected by the top two bits) instead of
// overlapping:
// +----------+---------------------+--------------------+
// |   type   |      module_id      |       offset       |
// +----------+---------------------+--------------------+
//  <- 2 bit -> <----- 30 bit -----> <----- 32 bit ----->
// 'type' is Object for code (a module wire-byte offset) and Memory for an
// offset into a module's linear memory.
enum class WasmAddressType : uint8_t {
  Memory = 0x00,
  Object = 0x01,
  Invalid = 0x03
};

class wasm_addr_t {
 public:
  // Addresses built by the stub (module bases, PCs, breakpoints) are code
  // addresses, i.e. module wire-byte offsets in the Object space.
  wasm_addr_t(uint32_t module_id, uint32_t offset)
      : offset_(offset),
        module_id_(module_id & 0x3fffffff),
        type_(static_cast<uint32_t>(WasmAddressType::Object)) {}
  explicit wasm_addr_t(uint64_t address)
      : offset_(static_cast<uint32_t>(address & 0xffffffff)),
        module_id_(static_cast<uint32_t>((address >> 32) & 0x3fffffff)),
        type_(static_cast<uint32_t>((address >> 62) & 0x3)) {}

  inline uint32_t ModuleId() const { return module_id_; }
  inline uint32_t Offset() const { return offset_; }
  inline WasmAddressType Type() const {
    return static_cast<WasmAddressType>(type_);
  }

  inline operator uint64_t() const {
    return (static_cast<uint64_t>(type_) << 62) |
           (static_cast<uint64_t>(module_id_) << 32) | offset_;
  }

 private:
  uint32_t offset_;
  uint32_t module_id_ : 30;
  uint32_t type_ : 2;
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_GDB_REMOTE_UTIL_H_
