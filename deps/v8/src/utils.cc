// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "platform.h"

#include "sys/stat.h"

namespace v8 {
namespace internal {


// Implementation is from "Hacker's Delight" by Henry S. Warren, Jr.,
// figure 3-3, page 48, where the function is called clp2.
uint32_t RoundUpToPowerOf2(uint32_t x) {
  ASSERT(x <= 0x80000000u);
  x = x - 1;
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x + 1;
}


// Thomas Wang, Integer Hash Functions.
// http://www.concentric.net/~Ttwang/tech/inthash.htm
uint32_t ComputeIntegerHash(uint32_t key) {
  uint32_t hash = key;
  hash = ~hash + (hash << 15);  // hash = (hash << 15) - hash - 1;
  hash = hash ^ (hash >> 12);
  hash = hash + (hash << 2);
  hash = hash ^ (hash >> 4);
  hash = hash * 2057;  // hash = (hash + (hash << 3)) + (hash << 11);
  hash = hash ^ (hash >> 16);
  return hash;
}


void PrintF(const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  OS::VPrint(format, arguments);
  va_end(arguments);
}


void Flush() {
  fflush(stdout);
}


char* ReadLine(const char* prompt) {
  char* result = NULL;
  char line_buf[256];
  int offset = 0;
  bool keep_going = true;
  fprintf(stdout, "%s", prompt);
  fflush(stdout);
  while (keep_going) {
    if (fgets(line_buf, sizeof(line_buf), stdin) == NULL) {
      // fgets got an error. Just give up.
      if (result != NULL) {
        DeleteArray(result);
      }
      return NULL;
    }
    int len = StrLength(line_buf);
    if (len > 1 &&
        line_buf[len - 2] == '\\' &&
        line_buf[len - 1] == '\n') {
      // When we read a line that ends with a "\" we remove the escape and
      // append the remainder.
      line_buf[len - 2] = '\n';
      line_buf[len - 1] = 0;
      len -= 1;
    } else if ((len > 0) && (line_buf[len - 1] == '\n')) {
      // Since we read a new line we are done reading the line. This
      // will exit the loop after copying this buffer into the result.
      keep_going = false;
    }
    if (result == NULL) {
      // Allocate the initial result and make room for the terminating '\0'
      result = NewArray<char>(len + 1);
    } else {
      // Allocate a new result with enough room for the new addition.
      int new_len = offset + len + 1;
      char* new_result = NewArray<char>(new_len);
      // Copy the existing input into the new array and set the new
      // array as the result.
      memcpy(new_result, result, offset * kCharSize);
      DeleteArray(result);
      result = new_result;
    }
    // Copy the newly read line into the result.
    memcpy(result + offset, line_buf, len * kCharSize);
    offset += len;
  }
  ASSERT(result != NULL);
  result[offset] = '\0';
  return result;
}


char* ReadCharsFromFile(const char* filename,
                        int* size,
                        int extra_space,
                        bool verbose) {
  FILE* file = OS::FOpen(filename, "rb");
  if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
    if (verbose) {
      OS::PrintError("Cannot read from file %s.\n", filename);
    }
    return NULL;
  }

  // Get the size of the file and rewind it.
  *size = ftell(file);
  rewind(file);

  char* result = NewArray<char>(*size + extra_space);
  for (int i = 0; i < *size;) {
    int read = static_cast<int>(fread(&result[i], 1, *size - i, file));
    if (read <= 0) {
      fclose(file);
      DeleteArray(result);
      return NULL;
    }
    i += read;
  }
  fclose(file);
  return result;
}


byte* ReadBytes(const char* filename, int* size, bool verbose) {
  char* chars = ReadCharsFromFile(filename, size, 0, verbose);
  return reinterpret_cast<byte*>(chars);
}


Vector<const char> ReadFile(const char* filename,
                            bool* exists,
                            bool verbose) {
  int size;
  char* result = ReadCharsFromFile(filename, &size, 1, verbose);
  if (!result) {
    *exists = false;
    return Vector<const char>::empty();
  }
  result[size] = '\0';
  *exists = true;
  return Vector<const char>(result, size);
}


int WriteCharsToFile(const char* str, int size, FILE* f) {
  int total = 0;
  while (total < size) {
    int write = static_cast<int>(fwrite(str, 1, size - total, f));
    if (write == 0) {
      return total;
    }
    total += write;
    str += write;
  }
  return total;
}


int WriteChars(const char* filename,
               const char* str,
               int size,
               bool verbose) {
  FILE* f = OS::FOpen(filename, "wb");
  if (f == NULL) {
    if (verbose) {
      OS::PrintError("Cannot open file %s for writing.\n", filename);
    }
    return 0;
  }
  int written = WriteCharsToFile(str, size, f);
  fclose(f);
  return written;
}


int WriteBytes(const char* filename,
               const byte* bytes,
               int size,
               bool verbose) {
  const char* str = reinterpret_cast<const char*>(bytes);
  return WriteChars(filename, str, size, verbose);
}


StringBuilder::StringBuilder(int size) {
  buffer_ = Vector<char>::New(size);
  position_ = 0;
}


void StringBuilder::AddString(const char* s) {
  AddSubstring(s, StrLength(s));
}


void StringBuilder::AddSubstring(const char* s, int n) {
  ASSERT(!is_finalized() && position_ + n < buffer_.length());
  ASSERT(static_cast<size_t>(n) <= strlen(s));
  memcpy(&buffer_[position_], s, n * kCharSize);
  position_ += n;
}


void StringBuilder::AddFormatted(const char* format, ...) {
  ASSERT(!is_finalized() && position_ < buffer_.length());
  va_list args;
  va_start(args, format);
  int n = OS::VSNPrintF(buffer_ + position_, format, args);
  va_end(args);
  if (n < 0 || n >= (buffer_.length() - position_)) {
    position_ = buffer_.length();
  } else {
    position_ += n;
  }
}


void StringBuilder::AddPadding(char c, int count) {
  for (int i = 0; i < count; i++) {
    AddCharacter(c);
  }
}


char* StringBuilder::Finalize() {
  ASSERT(!is_finalized() && position_ < buffer_.length());
  buffer_[position_] = '\0';
  // Make sure nobody managed to add a 0-character to the
  // buffer while building the string.
  ASSERT(strlen(buffer_.start()) == static_cast<size_t>(position_));
  position_ = -1;
  ASSERT(is_finalized());
  return buffer_.start();
}


int TenToThe(int exponent) {
  ASSERT(exponent <= 9);
  ASSERT(exponent >= 1);
  int answer = 10;
  for (int i = 1; i < exponent; i++) answer *= 10;
  return answer;
}

} }  // namespace v8::internal
