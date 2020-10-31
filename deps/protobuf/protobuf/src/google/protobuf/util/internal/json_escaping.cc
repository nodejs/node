// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/util/internal/json_escaping.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

namespace {

// Array of hex characters for conversion to hex.
static const char kHex[] = "0123456789abcdef";

// Characters 0x00 to 0x9f are very commonly used, so we provide a special
// table lookup.
//
// For unicode code point ch < 0xa0:
// kCommonEscapes[ch] is the escaped string of ch, if escaping is needed;
//                    or an empty string, if escaping is not needed.
static const char kCommonEscapes[160][7] = {
    // C0 (ASCII and derivatives) control characters
    "\\u0000", "\\u0001", "\\u0002", "\\u0003",  // 0x00
    "\\u0004", "\\u0005", "\\u0006", "\\u0007", "\\b", "\\t", "\\n", "\\u000b",
    "\\f", "\\r", "\\u000e", "\\u000f", "\\u0010", "\\u0011", "\\u0012",
    "\\u0013",  // 0x10
    "\\u0014", "\\u0015", "\\u0016", "\\u0017", "\\u0018", "\\u0019", "\\u001a",
    "\\u001b", "\\u001c", "\\u001d", "\\u001e", "\\u001f",
    // Escaping of " and \ are required by www.json.org string definition.
    // Escaping of < and > are required for HTML security.
    "", "", "\\\"", "", "", "", "", "",                              // 0x20
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",  // 0x30
    "", "", "", "", "\\u003c", "", "\\u003e", "", "", "", "", "", "", "", "",
    "",                                                                  // 0x40
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",      // 0x50
    "", "", "", "", "\\\\", "", "", "", "", "", "", "", "", "", "", "",  // 0x60
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",      // 0x70
    "", "", "", "", "", "", "", "\\u007f",
    // C1 (ISO 8859 and Unicode) extended control characters
    "\\u0080", "\\u0081", "\\u0082", "\\u0083",  // 0x80
    "\\u0084", "\\u0085", "\\u0086", "\\u0087", "\\u0088", "\\u0089", "\\u008a",
    "\\u008b", "\\u008c", "\\u008d", "\\u008e", "\\u008f", "\\u0090", "\\u0091",
    "\\u0092", "\\u0093",  // 0x90
    "\\u0094", "\\u0095", "\\u0096", "\\u0097", "\\u0098", "\\u0099", "\\u009a",
    "\\u009b", "\\u009c", "\\u009d", "\\u009e", "\\u009f"};

// Determines if the given char value is a unicode surrogate code unit (either
// high-surrogate or low-surrogate).
inline bool IsSurrogate(uint32 c) {
  // Optimized form of:
  // return c >= kMinHighSurrogate && c <= kMaxLowSurrogate;
  // (Reduced from 3 ALU instructions to 2 ALU instructions)
  return (c & 0xfffff800) == JsonEscaping::kMinHighSurrogate;
}

// Returns true if the given unicode code point cp is a valid
// unicode code point (i.e. in the range 0 <= cp <= kMaxCodePoint).
inline bool IsValidCodePoint(uint32 cp) {
  return cp <= JsonEscaping::kMaxCodePoint;
}

// Returns the low surrogate for the given unicode code point. The result is
// meaningless if the given code point is not a supplementary character.
inline uint16 ToLowSurrogate(uint32 cp) {
  return (cp &
          (JsonEscaping::kMaxLowSurrogate - JsonEscaping::kMinLowSurrogate)) +
         JsonEscaping::kMinLowSurrogate;
}

// Returns the high surrogate for the given unicode code point. The result is
// meaningless if the given code point is not a supplementary character.
inline uint16 ToHighSurrogate(uint32 cp) {
  return (cp >> 10) + (JsonEscaping::kMinHighSurrogate -
                       (JsonEscaping::kMinSupplementaryCodePoint >> 10));
}

// Input str is encoded in UTF-8. A unicode code point could be encoded in
// UTF-8 using anywhere from 1 to 4 characters, and it could span multiple
// reads of the ByteSource.
//
// This function reads the next unicode code point from the input (str) at
// the given position (index), taking into account any left-over partial
// code point from the previous iteration (cp), together with the number
// of characters left to read to complete this code point (num_left).
//
// This function assumes that the input (str) is valid at the given position
// (index). In order words, at least one character could be read successfully.
//
// The code point read (partial or complete) is stored in (cp). Upon return,
// (num_left) stores the number of characters that has yet to be read in
// order to complete the current unicode code point. If the read is complete,
// then (num_left) is 0. Also, (num_read) is the number of characters read.
//
// Returns false if we encounter an invalid UTF-8 string. Returns true
// otherwise, including the case when we reach the end of the input (str)
// before a complete unicode code point is read.
bool ReadCodePoint(StringPiece str, int index, uint32* cp, int* num_left,
                   int* num_read) {
  if (*num_left == 0) {
    // Last read was complete. Start reading a new unicode code point.
    *cp = static_cast<uint8>(str[index++]);
    *num_read = 1;
    // The length of the code point is determined from reading the first byte.
    //
    // If the first byte is between:
    //    0..0x7f: that's the value of the code point.
    // 0x80..0xbf: <invalid>
    // 0xc0..0xdf: 11-bit code point encoded in 2 bytes.
    //                                   bit 10-6, bit 5-0
    // 0xe0..0xef: 16-bit code point encoded in 3 bytes.
    //                        bit 15-12, bit 11-6, bit 5-0
    // 0xf0..0xf7: 21-bit code point encoded in 4 bytes.
    //             bit 20-18, bit 17-12, bit 11-6, bit 5-0
    // 0xf8..0xff: <invalid>
    //
    // Meaning of each bit:
    // <msb> bit 7: 0 - single byte code point: bits 6-0 are values.
    //              1 - multibyte code point
    //       bit 6: 0 - subsequent bytes of multibyte code point:
    //                  bits 5-0 are values.
    //              1 - first byte of multibyte code point
    //       bit 5: 0 - first byte of 2-byte code point: bits 4-0 are values.
    //              1 - first byte of code point with >= 3 bytes.
    //       bit 4: 0 - first byte of 3-byte code point: bits 3-0 are values.
    //              1 - first byte of code point with >= 4 bytes.
    //       bit 3: 0 - first byte of 4-byte code point: bits 2-0 are values.
    //              1 - reserved for future expansion.
    if (*cp <= 0x7f) {
      return true;
    } else if (*cp <= 0xbf) {
      return false;
    } else if (*cp <= 0xdf) {
      *cp &= 0x1f;
      *num_left = 1;
    } else if (*cp <= 0xef) {
      *cp &= 0x0f;
      *num_left = 2;
    } else if (*cp <= 0xf7) {
      *cp &= 0x07;
      *num_left = 3;
    } else {
      return false;
    }
  } else {
    // Last read was partial. Initialize num_read to 0 and continue reading
    // the last unicode code point.
    *num_read = 0;
  }
  while (*num_left > 0 && index < str.size()) {
    uint32 ch = static_cast<uint8>(str[index++]);
    --(*num_left);
    ++(*num_read);
    *cp = (*cp << 6) | (ch & 0x3f);
    if (ch < 0x80 || ch > 0xbf) return false;
  }
  return *num_left > 0 || (!IsSurrogate(*cp) && IsValidCodePoint(*cp));
}

// Stores the 16-bit unicode code point as its hexadecimal digits in buffer
// and returns a StringPiece that points to this buffer. The input buffer needs
// to be at least 6 bytes long.
StringPiece ToHex(uint16 cp, char* buffer) {
  buffer[5] = kHex[cp & 0x0f];
  cp >>= 4;
  buffer[4] = kHex[cp & 0x0f];
  cp >>= 4;
  buffer[3] = kHex[cp & 0x0f];
  cp >>= 4;
  buffer[2] = kHex[cp & 0x0f];
  return StringPiece(buffer, 6);
}

// Stores the 32-bit unicode code point as its hexadecimal digits in buffer
// and returns a StringPiece that points to this buffer. The input buffer needs
// to be at least 12 bytes long.
StringPiece ToSurrogateHex(uint32 cp, char* buffer) {
  uint16 low = ToLowSurrogate(cp);
  uint16 high = ToHighSurrogate(cp);

  buffer[11] = kHex[low & 0x0f];
  low >>= 4;
  buffer[10] = kHex[low & 0x0f];
  low >>= 4;
  buffer[9] = kHex[low & 0x0f];
  low >>= 4;
  buffer[8] = kHex[low & 0x0f];

  buffer[5] = kHex[high & 0x0f];
  high >>= 4;
  buffer[4] = kHex[high & 0x0f];
  high >>= 4;
  buffer[3] = kHex[high & 0x0f];
  high >>= 4;
  buffer[2] = kHex[high & 0x0f];

  return StringPiece(buffer, 12);
}

// If the given unicode code point needs escaping, then returns the
// escaped form. The returned StringPiece either points to statically
// pre-allocated char[] or to the given buffer. The input buffer needs
// to be at least 12 bytes long.
//
// If the given unicode code point does not need escaping, an empty
// StringPiece is returned.
StringPiece EscapeCodePoint(uint32 cp, char* buffer) {
  if (cp < 0xa0) return kCommonEscapes[cp];
  switch (cp) {
    // These are not required by json spec
    // but used to prevent security bugs in javascript.
    case 0xfeff:  // Zero width no-break space
    case 0xfff9:  // Interlinear annotation anchor
    case 0xfffa:  // Interlinear annotation separator
    case 0xfffb:  // Interlinear annotation terminator

    case 0x00ad:  // Soft-hyphen
    case 0x06dd:  // Arabic end of ayah
    case 0x070f:  // Syriac abbreviation mark
    case 0x17b4:  // Khmer vowel inherent Aq
    case 0x17b5:  // Khmer vowel inherent Aa
      return ToHex(cp, buffer);

    default:
      if ((cp >= 0x0600 && cp <= 0x0603) ||  // Arabic signs
          (cp >= 0x200b && cp <= 0x200f) ||  // Zero width etc.
          (cp >= 0x2028 && cp <= 0x202e) ||  // Separators etc.
          (cp >= 0x2060 && cp <= 0x2064) ||  // Invisible etc.
          (cp >= 0x206a && cp <= 0x206f)) {  // Shaping etc.
        return ToHex(cp, buffer);
      }

      if (cp == 0x000e0001 ||                        // Language tag
          (cp >= 0x0001d173 && cp <= 0x0001d17a) ||  // Music formatting
          (cp >= 0x000e0020 && cp <= 0x000e007f)) {  // TAG symbols
        return ToSurrogateHex(cp, buffer);
      }
  }
  return StringPiece();
}

// Tries to escape the given code point first. If the given code point
// does not need to be escaped, but force_output is true, then render
// the given multi-byte code point in UTF8 in the buffer and returns it.
StringPiece EscapeCodePoint(uint32 cp, char* buffer, bool force_output) {
  StringPiece sp = EscapeCodePoint(cp, buffer);
  if (force_output && sp.empty()) {
    buffer[5] = (cp & 0x3f) | 0x80;
    cp >>= 6;
    if (cp <= 0x1f) {
      buffer[4] = cp | 0xc0;
      sp = StringPiece(buffer + 4, 2);
      return sp;
    }
    buffer[4] = (cp & 0x3f) | 0x80;
    cp >>= 6;
    if (cp <= 0x0f) {
      buffer[3] = cp | 0xe0;
      sp = StringPiece(buffer + 3, 3);
      return sp;
    }
    buffer[3] = (cp & 0x3f) | 0x80;
    buffer[2] = ((cp >> 6) & 0x07) | 0xf0;
    sp = StringPiece(buffer + 2, 4);
  }
  return sp;
}

}  // namespace

void JsonEscaping::Escape(strings::ByteSource* input,
                          strings::ByteSink* output) {
  char buffer[12] = "\\udead\\ubee";
  uint32 cp = 0;     // Current unicode code point.
  int num_left = 0;  // Num of chars to read to complete the code point.
  while (input->Available() > 0) {
    StringPiece str = input->Peek();
    StringPiece escaped;
    int i = 0;
    int num_read;
    bool ok;
    bool cp_was_split = num_left > 0;
    // Loop until we encounter either
    //   i) a code point that needs to be escaped; or
    //  ii) a split code point is completely read; or
    // iii) a character that is not a valid utf8; or
    //  iv) end of the StringPiece str is reached.
    do {
      ok = ReadCodePoint(str, i, &cp, &num_left, &num_read);
      if (num_left > 0 || !ok) break;  // case iii or iv
      escaped = EscapeCodePoint(cp, buffer, cp_was_split);
      if (!escaped.empty()) break;  // case i or ii
      i += num_read;
      num_read = 0;
    } while (i < str.length());  // case iv
    // First copy the un-escaped prefix, if any, to the output ByteSink.
    if (i > 0) input->CopyTo(output, i);
    if (num_read > 0) input->Skip(num_read);
    if (!ok) {
      // Case iii: Report error.
      // TODO(wpoon): Add error reporting.
      num_left = 0;
    } else if (num_left == 0 && !escaped.empty()) {
      // Case i or ii: Append the escaped code point to the output ByteSink.
      output->Append(escaped.data(), escaped.size());
    }
  }
  if (num_left > 0) {
    // Treat as case iii: report error.
    // TODO(wpoon): Add error reporting.
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
