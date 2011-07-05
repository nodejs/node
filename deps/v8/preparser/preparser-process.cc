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

#include <stdlib.h>
#include <stdarg.h>
#include "../include/v8stdint.h"
#include "../include/v8-preparser.h"
#include "unicode-inl.h"

enum ResultCode { kSuccess = 0, kErrorReading = 1, kErrorWriting = 2 };

namespace v8 {
namespace internal {

// THIS FILE IS PROOF-OF-CONCEPT ONLY.
// The final goal is a stand-alone preparser library.


class UTF8InputStream : public v8::UnicodeInputStream {
 public:
  UTF8InputStream(uint8_t* buffer, size_t length)
      : buffer_(buffer),
        offset_(0),
        pos_(0),
        end_offset_(static_cast<int>(length)) { }

  virtual ~UTF8InputStream() { }

  virtual void PushBack(int32_t ch) {
    // Pushback assumes that the character pushed back is the
    // one that was most recently read, and jumps back in the
    // UTF-8 stream by the length of that character's encoding.
    offset_ -= unibrow::Utf8::Length(ch);
    pos_--;
#ifdef DEBUG
    if (static_cast<unsigned>(ch) <= unibrow::Utf8::kMaxOneByteChar) {
      if (ch != buffer_[offset_]) {
        fprintf(stderr, "Invalid pushback: '%c'.", ch);
        exit(1);
      }
    } else {
      unsigned tmp = 0;
      if (static_cast<unibrow::uchar>(ch) !=
          unibrow::Utf8::CalculateValue(buffer_ + offset_,
                                        end_offset_ - offset_,
                                        &tmp)) {
        fprintf(stderr, "Invalid pushback: 0x%x.", ch);
        exit(1);
      }
    }
#endif
  }

  virtual int32_t Next() {
    if (offset_ == end_offset_) return -1;
    uint8_t first_char = buffer_[offset_];
    if (first_char <= unibrow::Utf8::kMaxOneByteChar) {
      pos_++;
      offset_++;
      return static_cast<int32_t>(first_char);
    }
    unibrow::uchar codepoint =
        unibrow::Utf8::CalculateValue(buffer_ + offset_,
                                      end_offset_ - offset_,
                                      &offset_);
    pos_++;
    return static_cast<int32_t>(codepoint);
  }

 private:
  const uint8_t* buffer_;
  unsigned offset_;
  unsigned pos_;
  unsigned end_offset_;
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
  size_t actually_read = fread(buffer, 1, length, source);
  return (actually_read == length);
}


bool WriteBuffer(FILE* dest, const void* buffer, size_t length) {
  size_t actually_written = fwrite(buffer, 1, length, dest);
  return (actually_written == length);
}


template <typename T>
class ScopedPointer {
 public:
  explicit ScopedPointer(T* pointer) : pointer_(pointer) {}
  ~ScopedPointer() { delete[] pointer_; }
  T& operator[](int index) { return pointer_[index]; }
  T* operator*() { return pointer_ ;}
 private:
  T* pointer_;
};


// Preparse input and output result on stdout.
int PreParseIO(FILE* input) {
  fprintf(stderr, "LOG: Enter parsing loop\n");
  bool ok = true;
  uint32_t length = ReadUInt32(input, &ok);
  fprintf(stderr, "LOG: Input length: %d\n", length);
  if (!ok) return kErrorReading;
  ScopedPointer<uint8_t> buffer(new uint8_t[length]);

  if (!ReadBuffer(input, *buffer, length)) {
    return kErrorReading;
  }
  UTF8InputStream input_buffer(*buffer, static_cast<size_t>(length));

  v8::PreParserData data =
      v8::Preparse(&input_buffer, 64 * 1024 * sizeof(void*));  // NOLINT
  if (data.stack_overflow()) {
    fprintf(stderr, "LOG: Stack overflow\n");
    fflush(stderr);
    // Report stack overflow error/no-preparser-data.
    WriteUInt32(stdout, 0, &ok);
    if (!ok) return kErrorWriting;
    return 0;
  }

  uint32_t size = data.size();
  fprintf(stderr, "LOG: Success, data size: %u\n", size);
  fflush(stderr);
  WriteUInt32(stdout, size, &ok);
  if (!ok) return kErrorWriting;
  if (!WriteBuffer(stdout, data.data(), size)) {
    return kErrorWriting;
  }
  return 0;
}

} }  // namespace v8::internal


int main(int argc, char* argv[]) {
  FILE* input = stdin;
  if (argc > 1) {
    char* arg = argv[1];
    input = fopen(arg, "rb");
    if (input == NULL) return EXIT_FAILURE;
  }
  int status = 0;
  do {
    status = v8::internal::PreParseIO(input);
  } while (status == 0);
  fprintf(stderr, "EXIT: Failure %d\n", status);
  fflush(stderr);
  return EXIT_FAILURE;
}
