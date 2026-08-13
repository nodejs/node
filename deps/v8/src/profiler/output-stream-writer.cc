// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/output-stream-writer.h"

#include "src/strings/unicode-inl.h"

namespace v8 {
namespace internal {

namespace {

void WriteUChar(OutputStreamWriter* w, unibrow::uchar u) {
  static const char hex_chars[] = "0123456789ABCDEF";
  w->AddString("\\u");
  w->AddCharacter(hex_chars[(u >> 12) & 0xF]);
  w->AddCharacter(hex_chars[(u >> 8) & 0xF]);
  w->AddCharacter(hex_chars[(u >> 4) & 0xF]);
  w->AddCharacter(hex_chars[u & 0xF]);
}

}  // namespace

void OutputStreamWriter::AddJsonEscapedString(const unsigned char* s) {
  AddCharacter('\"');
  for (; *s != '\0'; ++s) {
    switch (*s) {
      case '\b':
        AddString("\\b");
        continue;
      case '\f':
        AddString("\\f");
        continue;
      case '\n':
        AddString("\\n");
        continue;
      case '\r':
        AddString("\\r");
        continue;
      case '\t':
        AddString("\\t");
        continue;
      case '\"':
      case '\\':
        AddCharacter('\\');
        AddCharacter(*s);
        continue;
      default:
        if (*s > 31 && *s < 128) {
          AddCharacter(*s);
        } else if (*s <= 31) {
          // Special character with no dedicated literal.
          WriteUChar(this, *s);
        } else {
          // Convert UTF-8 into \u UTF-16 literal.
          size_t length = 1, cursor = 0;
          for (; length <= 4 && *(s + length) != '\0'; ++length) {
          }
          unibrow::uchar c = unibrow::Utf8::CalculateValue(s, length, &cursor);
          if (c != unibrow::Utf8::kBadChar) {
            WriteUChar(this, c);
            DCHECK_NE(cursor, 0);
            s += cursor - 1;
          } else {
            AddCharacter('?');
          }
        }
    }
  }
  AddCharacter('\"');
}

}  // namespace internal
}  // namespace v8
