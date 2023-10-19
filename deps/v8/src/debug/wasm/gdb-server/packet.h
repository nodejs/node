// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_PACKET_H_
#define V8_DEBUG_WASM_GDB_SERVER_PACKET_H_

#include <string>

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

class V8_EXPORT_PRIVATE Packet {
 public:
  Packet();

  // Empty the vector and reset the read/write pointers.
  void Clear();

  // Reset the read pointer, allowing the packet to be re-read.
  void Rewind();

  // Return true of the read pointer has reached the write pointer.
  bool EndOfPacket() const;

  // Store a single raw 8 bit value
  void AddRawChar(char ch);

  // Store a block of data as hex pairs per byte
  void AddBlock(const void* ptr, uint32_t len);

  // Store a byte as a 2 chars block.
  void AddWord8(uint8_t val);

  // Store a number up to 64 bits, formatted as a big-endian hex string with
  // preceeding zeros removed.  Since zeros can be removed, the width of this
  // number is unknown, and the number is always followed by a NULL or a
  // separator (non hex digit).
  void AddNumberSep(uint64_t val, char sep);

  // Add a raw string.
  void AddString(const char* str);

  // Add a string stored as a stream of ASCII hex digit pairs.  It is safe
  // to use any non-null character in this stream.  If this does not terminate
  // the packet, there should be a separator (non hex digit) immediately
  // following.
  void AddHexString(const char* str);

  // Retrieve a single character if available
  bool GetRawChar(char* ch);

  // Retrieve "len" ASCII character pairs.
  bool GetBlock(void* ptr, uint32_t len);

  // Retrieve a 8, 16, 32, or 64 bit word as pairs of hex digits.  These
  // functions will always consume bits/4 characters from the stream.
  bool GetWord8(uint8_t* val);

  // Retrieve a number (formatted as a big-endian hex string) and a separator.
  // If 'sep' is null, the separator is consumed but thrown away.
  bool GetNumberSep(uint64_t* val, char* sep);

  // Get a string from the stream
  bool GetString(std::string* str);
  bool GetHexString(std::string* str);

  // Return a pointer to the entire packet payload
  const char* GetPayload() const;
  size_t GetPayloadSize() const;

  // Returns true and the sequence number, or false if it is unset.
  bool GetSequence(int32_t* seq) const;

  // Parses sequence number in package data and moves read pointer past it.
  void ParseSequence();

  // Set the sequence number.
  void SetSequence(int32_t seq);

  enum class ErrDef { None = 0, BadFormat = 1, BadArgs = 2, Failed = 3 };
  void SetError(ErrDef);

  // Returns the full content of a GDB-remote packet, in the format:
  //    $payload#checksum
  // where the two-digit checksum is computed as the modulo 256 sum of all
  // characters between the leading ‘$’ and the trailing ‘#’.
  std::string GetPacketData() const;

 private:
  int32_t seq_;
  std::string data_;
  size_t read_index_;
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_PACKET_H_
