// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/packet.h"
#include "src/debug/wasm/gdb-server/gdb-remote-util.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

Packet::Packet() {
  seq_ = -1;
  Clear();
}

void Packet::Clear() {
  data_.clear();
  read_index_ = 0;
}

void Packet::Rewind() { read_index_ = 0; }

bool Packet::EndOfPacket() const { return (read_index_ >= GetPayloadSize()); }

void Packet::AddRawChar(char ch) { data_.push_back(ch); }

void Packet::AddWord8(uint8_t byte) {
  char seq[2];
  UInt8ToHex(byte, seq);
  AddRawChar(seq[0]);
  AddRawChar(seq[1]);
}

void Packet::AddBlock(const void* ptr, uint32_t len) {
  DCHECK(ptr);

  const char* p = (const char*)ptr;

  for (uint32_t offs = 0; offs < len; offs++) {
    AddWord8(p[offs]);
  }
}

void Packet::AddString(const char* str) {
  DCHECK(str);

  while (*str) {
    AddRawChar(*str);
    str++;
  }
}

void Packet::AddHexString(const char* str) {
  DCHECK(str);

  while (*str) {
    AddWord8(*str);
    str++;
  }
}

void Packet::AddNumberSep(uint64_t val, char sep) {
  char out[sizeof(val) * 2];
  char temp[2];

  // Check for -1 optimization
  if (val == static_cast<uint64_t>(-1)) {
    AddRawChar('-');
    AddRawChar('1');
  } else {
    int nibbles = 0;

    // In the GDB remote protocol numbers are formatted as big-endian hex
    // strings. Leading zeros can be skipped.
    // For example the value 0x00001234 is formatted as "1234".
    for (size_t a = 0; a < sizeof(val); a++) {
      uint8_t byte = static_cast<uint8_t>(val & 0xFF);

      // Stream in with bytes reversed, starting with the least significant.
      // So if we have the value 0x00001234, we store 4, then 3, 2, 1.
      // Note that the characters are later reversed to be in big-endian order.
      UInt8ToHex(byte, temp);
      out[nibbles++] = temp[1];
      out[nibbles++] = temp[0];

      // Get the next 8 bits;
      val >>= 8;

      // Suppress leading zeros, so we are done when val hits zero
      if (val == 0) {
        break;
      }
    }

    // Strip the high zero for this byte if present.
    if ((nibbles > 1) && (out[nibbles - 1] == '0')) nibbles--;

    // Now write it out reverse to correct the order
    while (nibbles) {
      nibbles--;
      AddRawChar(out[nibbles]);
    }
  }

  // If we asked for a separator, insert it
  if (sep) AddRawChar(sep);
}

bool Packet::GetNumberSep(uint64_t* val, char* sep) {
  uint64_t out = 0;
  char ch;
  if (!GetRawChar(&ch)) {
    return false;
  }

  // Numbers are formatted as a big-endian hex strings.
  // The literals "0" and "-1" as special cases.

  // Check for -1
  if (ch == '-') {
    if (!GetRawChar(&ch)) {
      return false;
    }

    if (ch == '1') {
      *val = (uint64_t)-1;

      ch = 0;
      GetRawChar(&ch);
      if (sep) {
        *sep = ch;
      }
      return true;
    }
    return false;
  }

  do {
    uint8_t nib;

    // Check for separator
    if (!NibbleToUInt8(ch, &nib)) {
      break;
    }

    // Add this nibble.
    out = (out << 4) + nib;

    // Get the next character (if availible)
    ch = 0;
    if (!GetRawChar(&ch)) {
      break;
    }
  } while (1);

  // Set the value;
  *val = out;

  // Add the separator if the user wants it...
  if (sep != nullptr) *sep = ch;

  return true;
}

bool Packet::GetRawChar(char* ch) {
  DCHECK(ch != nullptr);

  if (read_index_ >= GetPayloadSize()) return false;

  *ch = data_[read_index_++];

  // Check for RLE X*N, where X is the value, N is the reps.
  if (*ch == '*') {
    if (read_index_ < 2) {
      TRACE_GDB_REMOTE("Unexpected RLE at start of packet.\n");
      return false;
    }

    if (read_index_ >= GetPayloadSize()) {
      TRACE_GDB_REMOTE("Unexpected EoP during RLE.\n");
      return false;
    }

    // GDB does not use "CTRL" characters in the stream, so the
    // number of reps is encoded as the ASCII value beyond 28
    // (which when you add a min rep size of 4, forces the rep
    // character to be ' ' (32) or greater).
    int32_t cnt = (data_[read_index_] - 28);
    if (cnt < 3) {
      TRACE_GDB_REMOTE("Unexpected RLE length.\n");
      return false;
    }

    // We have just read '*' and incremented the read pointer,
    // so here is the old state, and expected new state.
    //
    //   Assume N = 5, we grow by N - size of encoding (3).
    //
    // OldP:       R  W
    // OldD:  012X*N89 = 8 chars
    // Size:  012X*N89__ = 10 chars
    // Move:  012X*__N89 = 10 chars
    // Fill:  012XXXXX89 = 10 chars
    // NewP:       R    W  (shifted 5 - 3)

    // First, store the remaining characters to the right into a temp string.
    std::string right = data_.substr(read_index_ + 1);
    // Discard the '*' we just read
    data_.erase(read_index_ - 1);
    // Append (N-1) 'X' chars
    *ch = data_[read_index_ - 2];
    data_.append(cnt - 1, *ch);
    // Finally, append the remaining characters
    data_.append(right);
  }
  return true;
}

bool Packet::GetWord8(uint8_t* value) {
  DCHECK(value);

  // Get two ASCII hex values and convert them to ints
  char seq[2];
  if (!GetRawChar(&seq[0]) || !GetRawChar(&seq[1])) {
    return false;
  }
  return HexToUInt8(seq, value);
}

bool Packet::GetBlock(void* ptr, uint32_t len) {
  DCHECK(ptr);

  uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
  bool res = true;

  for (uint32_t offs = 0; offs < len; offs++) {
    res = GetWord8(&p[offs]);
    if (false == res) {
      break;
    }
  }

  return res;
}

bool Packet::GetString(std::string* str) {
  if (EndOfPacket()) {
    return false;
  }

  *str = data_.substr(read_index_);
  read_index_ = GetPayloadSize();
  return true;
}

bool Packet::GetHexString(std::string* str) {
  // Decode a string encoded as a series of 2-hex digit pairs.

  if (EndOfPacket()) {
    return false;
  }

  // Pull values until we hit a separator
  str->clear();
  char ch1;
  while (GetRawChar(&ch1)) {
    uint8_t nib1;
    if (!NibbleToUInt8(ch1, &nib1)) {
      read_index_--;
      break;
    }
    char ch2;
    uint8_t nib2;
    if (!GetRawChar(&ch2) || !NibbleToUInt8(ch2, &nib2)) {
      return false;
    }
    *str += static_cast<char>((nib1 << 4) + nib2);
  }
  return true;
}

const char* Packet::GetPayload() const { return data_.c_str(); }

size_t Packet::GetPayloadSize() const { return data_.size(); }

bool Packet::GetSequence(int32_t* ch) const {
  DCHECK(ch);

  if (seq_ != -1) {
    *ch = seq_;
    return true;
  }

  return false;
}

void Packet::ParseSequence() {
  size_t saved_read_index = read_index_;
  unsigned char seq;
  char ch;
  if (GetWord8(&seq) && GetRawChar(&ch)) {
    if (ch == ':') {
      SetSequence(seq);
      return;
    }
  }
  // No sequence number present, so reset to original position.
  read_index_ = saved_read_index;
}

void Packet::SetSequence(int32_t val) { seq_ = val; }

void Packet::SetError(ErrDef error) {
  Clear();
  AddRawChar('E');
  AddWord8(static_cast<uint8_t>(error));
}

std::string Packet::GetPacketData() const {
  char chars[2];
  const char* ptr = GetPayload();
  size_t size = GetPayloadSize();

  std::stringstream outstr;

  // Signal start of response
  outstr << '$';

  char run_xsum = 0;

  // If there is a sequence, send as two nibble 8bit value + ':'
  int32_t seq;
  if (GetSequence(&seq)) {
    UInt8ToHex(seq, chars);
    outstr << chars[0];
    run_xsum += chars[0];
    outstr << chars[1];
    run_xsum += chars[1];

    outstr << ':';
    run_xsum += ':';
  }

  // Send the main payload
  for (size_t offs = 0; offs < size; ++offs) {
    outstr << ptr[offs];
    run_xsum += ptr[offs];
  }

  // Send XSUM as two nibble 8bit value preceeded by '#'
  outstr << '#';
  UInt8ToHex(run_xsum, chars);
  outstr << chars[0];
  outstr << chars[1];

  return outstr.str();
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
