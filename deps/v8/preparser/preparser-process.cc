// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include <stdarg.h>
#include "../include/v8stdint.h"
#include "globals.h"
#include "checks.h"
#include "allocation.h"
#include "utils.h"
#include "list.h"
#include "smart-pointer.h"
#include "scanner-base.h"
#include "preparse-data.h"
#include "preparser.h"

enum ResultCode { kSuccess = 0, kErrorReading = 1, kErrorWriting = 2 };

namespace v8 {
namespace internal {

// THIS FILE IS PROOF-OF-CONCEPT ONLY.
// The final goal is a stand-alone preparser library.

// UTF16Buffer based on an UTF-8 string in memory.
class UTF8UTF16Buffer : public UTF16Buffer {
 public:
  UTF8UTF16Buffer(uint8_t* buffer, size_t length)
      : UTF16Buffer(),
        buffer_(buffer),
        offset_(0),
        end_offset_(static_cast<int>(length)) { }

  virtual void PushBack(uc32 ch) {
    // Pushback assumes that the character pushed back is the
    // one that was most recently read, and jumps back in the
    // UTF-8 stream by the length of that character's encoding.
    offset_ -= unibrow::Utf8::Length(ch);
    pos_--;
#ifdef DEBUG
    int tmp = 0;
    ASSERT_EQ(ch, unibrow::Utf8::ValueOf(buffer_ + offset_,
                                         end_offset_ - offset_,
                                         &tmp);
#endif
  }

  virtual uc32 Advance() {
    if (offset_ == end_offset_) return -1;
    uint8_t first_char = buffer_[offset_];
    if (first_char <= unibrow::Utf8::kMaxOneByteChar) {
      pos_++;
      offset_++;
      return static_cast<uc32>(first_char);
    }
    unibrow::uchar codepoint =
        unibrow::Utf8::CalculateValue(buffer_ + offset_,
                                      end_offset_ - offset_,
                                      &offset_);
    pos_++;
    return static_cast<uc32>(codepoint);
  }

  virtual void SeekForward(int pos) {
    while (pos_ < pos) {
      uint8_t first_byte = buffer_[offset_++];
      while (first_byte & 0x80u && offset_ < end_offset_) {
        offset_++;
        first_byte <<= 1;
      }
      pos_++;
    }
  }

 private:
  const uint8_t* buffer_;
  unsigned offset_;
  unsigned end_offset_;
};


class StandAloneJavaScriptScanner : public JavaScriptScanner {
 public:
  void Initialize(UTF16Buffer* source) {
    source_ = source;
    literal_flags_ = kLiteralString | kLiteralIdentifier;
    Init();
    // Skip initial whitespace allowing HTML comment ends just like
    // after a newline and scan first token.
    has_line_terminator_before_next_ = true;
    SkipWhiteSpace();
    Scan();
  }
};


// Write a number to dest in network byte order.
void WriteUInt32(FILE* dest, uint32_t value, bool* ok) {
  for (int i = 3; i >= 0; i--) {
    uint8_t byte = static_cast<uint8_t>(value >> (i << 3));
    int result = fputc(byte, dest);
    if (result == EOF) {
      *ok = false;
      return;
    }
  }
}

// Read number from FILE* in network byte order.
uint32_t ReadUInt32(FILE* source, bool* ok) {
  uint32_t n = 0;
  for (int i = 0; i < 4; i++) {
    int c = fgetc(source);
    if (c == EOF) {
      *ok = false;
      return 0;
    }
    n = (n << 8) + static_cast<uint32_t>(c);
  }
  return n;
}


bool ReadBuffer(FILE* source, void* buffer, size_t length) {
  size_t actually_read = fread(buffer, 1, length, stdin);
  return (actually_read == length);
}


bool WriteBuffer(FILE* dest, void* buffer, size_t length) {
  size_t actually_written = fwrite(buffer, 1, length, dest);
  return (actually_written == length);
}

// Preparse stdin and output result on stdout.
int PreParseIO() {
  fprintf(stderr, "LOG: Enter parsing loop\n");
  bool ok = true;
  uint32_t length = ReadUInt32(stdin, &ok);
  if (!ok) return kErrorReading;
  SmartPointer<byte> buffer(NewArray<byte>(length));
  if (!ReadBuffer(stdin, *buffer, length)) {
    return kErrorReading;
  }
  UTF8UTF16Buffer input_buffer(*buffer, static_cast<size_t>(length));
  StandAloneJavaScriptScanner scanner;
  scanner.Initialize(&input_buffer);
  CompleteParserRecorder recorder;
  preparser::PreParser preparser;

  if (!preparser.PreParseProgram(&scanner, &recorder, true)) {
    if (scanner.stack_overflow()) {
      // Report stack overflow error/no-preparser-data.
      WriteUInt32(stdout, 0, &ok);
      if (!ok) return kErrorWriting;
      return 0;
    }
  }
  Vector<unsigned> pre_data = recorder.ExtractData();

  uint32_t size = static_cast<uint32_t>(pre_data.length() * sizeof(uint32_t));
  WriteUInt32(stdout, size, &ok);
  if (!ok) return kErrorWriting;
  if (!WriteBuffer(stdout,
                   reinterpret_cast<byte*>(pre_data.start()),
                   size)) {
    return kErrorWriting;
  }
  return 0;
}

// Functions declared by allocation.h

void FatalProcessOutOfMemory(const char* location) {
  V8_Fatal("", 0, location);
}

bool EnableSlowAsserts() { return true; }

} }  // namespace v8::internal


int main(int argc, char* argv[]) {
  int status = 0;
  do {
    status = v8::internal::PreParseIO();
  } while (status == 0);
  fprintf(stderr, "EXIT: Failure %d\n", status);
  return EXIT_FAILURE;
}


// Fatal error handling declared by checks.h.

extern "C" void V8_Fatal(const char* file, int line, const char* format, ...) {
  fflush(stdout);
  fflush(stderr);
  va_list arguments;
  va_start(arguments, format);
  vfprintf(stderr, format, arguments);
  va_end(arguments);
  fputs("\n#\n\n", stderr);
  exit(EXIT_FAILURE);
}
