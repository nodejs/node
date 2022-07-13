// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/gdb-remote-util.h"
using std::string;

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

// GDB expects lower case values.
static const char kHexChars[] = "0123456789abcdef";

void UInt8ToHex(uint8_t byte, char chars[2]) {
  DCHECK(chars);
  chars[0] = kHexChars[byte >> 4];
  chars[1] = kHexChars[byte & 0xF];
}

bool HexToUInt8(const char chars[2], uint8_t* byte) {
  uint8_t o1, o2;
  if (NibbleToUInt8(chars[0], &o1) && NibbleToUInt8(chars[1], &o2)) {
    *byte = (o1 << 4) + o2;
    return true;
  }

  return false;
}

bool NibbleToUInt8(char ch, uint8_t* byte) {
  DCHECK(byte);

  // Check for nibble of a-f
  if ((ch >= 'a') && (ch <= 'f')) {
    *byte = (ch - 'a' + 10);
    return true;
  }

  // Check for nibble of A-F
  if ((ch >= 'A') && (ch <= 'F')) {
    *byte = (ch - 'A' + 10);
    return true;
  }

  // Check for nibble of 0-9
  if ((ch >= '0') && (ch <= '9')) {
    *byte = (ch - '0');
    return true;
  }

  // Not a valid nibble representation
  return false;
}

std::vector<std::string> StringSplit(const string& instr, const char* delim) {
  std::vector<std::string> result;

  const char* in = instr.data();
  if (nullptr == in) return result;

  // Check if we have nothing to do
  if (nullptr == delim) {
    result.push_back(string(in));
    return result;
  }

  while (*in) {
    // Toss all preceeding delimiters
    while (*in && strchr(delim, *in)) in++;

    // If we still have something to process
    if (*in) {
      const char* start = in;
      size_t len = 0;
      // Keep moving forward for all valid chars
      while (*in && (strchr(delim, *in) == nullptr)) {
        len++;
        in++;
      }

      // Build this token and add it to the array.
      result.push_back(string{start, len});
    }
  }
  return result;
}

std::string Mem2Hex(const uint8_t* mem, size_t count) {
  std::vector<char> result(count * 2 + 1);
  for (size_t i = 0; i < count; i++) UInt8ToHex(*mem++, &result[i * 2]);
  result[count * 2] = '\0';
  return result.data();
}

std::string Mem2Hex(const std::string& str) {
  return Mem2Hex(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
