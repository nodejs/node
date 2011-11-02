// Copyright 2011 the V8 project authors. All rights reserved.
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
#include <stdio.h>
#include <string.h>

#include "../include/v8stdint.h"
#include "../include/v8-preparser.h"

#include "../src/preparse-data-format.h"

namespace i = v8::internal;

// This file is only used for testing the stand-alone preparser
// library.
// The first argument must be the path of a JavaScript source file, or
// the flags "-e" and the next argument is then the source of a JavaScript
// program.
// Optionally this can be followed by the word "throws" (case sensitive),
// which signals that the parsing is expected to throw - the default is
// to expect the parsing to not throw.
// The command line can further be followed by a message text (the
// *type* of the exception to throw), and even more optionally, the
// start and end position reported with the exception.
//
// This source file is preparsed and tested against the expectations, and if
// successful, the resulting preparser data is written to stdout.
// Diagnostic output is output on stderr.
// The source file must contain only ASCII characters (UTF-8 isn't supported).
// The file is read into memory, so it should have a reasonable size.


// Adapts an ASCII string to the UnicodeInputStream interface.
class AsciiInputStream : public v8::UnicodeInputStream {
 public:
  AsciiInputStream(const uint8_t* buffer, size_t length)
      : buffer_(buffer),
        end_offset_(static_cast<int>(length)),
        offset_(0) { }

  virtual ~AsciiInputStream() { }

  virtual void PushBack(int32_t ch) {
    offset_--;
#ifdef DEBUG
    if (offset_ < 0 ||
        (ch != ((offset_ >= end_offset_) ? -1 : buffer_[offset_]))) {
      fprintf(stderr, "Invalid pushback: '%c' at offset %d.", ch, offset_);
      exit(1);
    }
#endif
  }

  virtual int32_t Next() {
    if (offset_ >= end_offset_) {
      offset_++;  // Increment anyway to allow symmetric pushbacks.
      return -1;
    }
    uint8_t next_char = buffer_[offset_];
#ifdef DEBUG
    if (next_char > 0x7fu) {
      fprintf(stderr, "Non-ASCII character in input: '%c'.", next_char);
      exit(1);
    }
#endif
    offset_++;
    return static_cast<int32_t>(next_char);
  }

 private:
  const uint8_t* buffer_;
  const int end_offset_;
  int offset_;
};


bool ReadBuffer(FILE* source, void* buffer, size_t length) {
  size_t actually_read = fread(buffer, 1, length, source);
  return (actually_read == length);
}


bool WriteBuffer(FILE* dest, const void* buffer, size_t length) {
  size_t actually_written = fwrite(buffer, 1, length, dest);
  return (actually_written == length);
}


class PreparseDataInterpreter {
 public:
  PreparseDataInterpreter(const uint8_t* data, int length)
      : data_(data), length_(length), message_(NULL) { }

  ~PreparseDataInterpreter() {
    if (message_ != NULL) delete[] message_;
  }

  bool valid() {
    int header_length =
      i::PreparseDataConstants::kHeaderSize * sizeof(int);  // NOLINT
    return length_ >= header_length;
  }

  bool throws() {
    return valid() &&
        word(i::PreparseDataConstants::kHasErrorOffset) != 0;
  }

  const char* message() {
    if (message_ != NULL) return message_;
    if (!throws()) return NULL;
    int text_pos = i::PreparseDataConstants::kHeaderSize +
                   i::PreparseDataConstants::kMessageTextPos;
    int length = word(text_pos);
    char* buffer = new char[length + 1];
    for (int i = 1; i <= length; i++) {
      int character = word(text_pos + i);
      buffer[i - 1] = character;
    }
    buffer[length] = '\0';
    message_ = buffer;
    return buffer;
  }

  int beg_pos() {
    if (!throws()) return -1;
    return word(i::PreparseDataConstants::kHeaderSize +
                i::PreparseDataConstants::kMessageStartPos);
  }

  int end_pos() {
    if (!throws()) return -1;
    return word(i::PreparseDataConstants::kHeaderSize +
                i::PreparseDataConstants::kMessageEndPos);
  }

 private:
  int word(int offset) {
    const int* word_data = reinterpret_cast<const int*>(data_);
    if (word_data + offset < reinterpret_cast<const int*>(data_ + length_)) {
      return word_data[offset];
    }
    return -1;
  }

  const uint8_t* const data_;
  const int length_;
  const char* message_;
};


template <typename T>
class ScopedPointer {
 public:
  explicit ScopedPointer() : pointer_(NULL) {}
  explicit ScopedPointer(T* pointer) : pointer_(pointer) {}
  ~ScopedPointer() { if (pointer_ != NULL) delete[] pointer_; }
  T& operator[](int index) { return pointer_[index]; }
  T* operator*() { return pointer_ ;}
  T* operator=(T* new_value) {
    if (pointer_ != NULL) delete[] pointer_;
    pointer_ = new_value;
    return new_value;
  }
 private:
  T* pointer_;
};



void fail(v8::PreParserData* data, const char* message, ...) {
  va_list args;
  va_start(args, message);
  vfprintf(stderr, message, args);
  va_end(args);
  fflush(stderr);
  // Print preparser data to stdout.
  uint32_t size = data->size();
  fprintf(stderr, "LOG: data size: %u\n", size);
  if (!WriteBuffer(stdout, data->data(), size)) {
    perror("ERROR: Writing data");
    fflush(stderr);
  }
  exit(EXIT_FAILURE);
}


bool IsFlag(const char* arg) {
  // Anything starting with '-' is considered a flag.
  // It's summarily ignored for now.
  return arg[0] == '-';
}


struct ExceptionExpectation {
  ExceptionExpectation()
      : throws(false), type(NULL), beg_pos(-1), end_pos(-1) { }
  bool throws;
  const char* type;
  int beg_pos;
  int end_pos;
};


void CheckException(v8::PreParserData* data,
                    ExceptionExpectation* expects) {
  PreparseDataInterpreter reader(data->data(), data->size());
  if (expects->throws) {
    if (!reader.throws()) {
      if (expects->type == NULL) {
        fail(data, "Didn't throw as expected\n");
      } else {
        fail(data, "Didn't throw \"%s\" as expected\n", expects->type);
      }
    }
    if (expects->type != NULL) {
      const char* actual_message = reader.message();
      if (strcmp(expects->type, actual_message)) {
        fail(data, "Wrong error message. Expected <%s>, found <%s> at %d..%d\n",
             expects->type, actual_message, reader.beg_pos(), reader.end_pos());
      }
    }
    if (expects->beg_pos >= 0) {
      if (expects->beg_pos != reader.beg_pos()) {
        fail(data, "Wrong error start position: Expected %i, found %i\n",
             expects->beg_pos, reader.beg_pos());
      }
    }
    if (expects->end_pos >= 0) {
      if (expects->end_pos != reader.end_pos()) {
        fail(data, "Wrong error end position: Expected %i, found %i\n",
             expects->end_pos, reader.end_pos());
      }
    }
  } else if (reader.throws()) {
    const char* message = reader.message();
    fail(data, "Throws unexpectedly with message: %s at location %d-%d\n",
         message, reader.beg_pos(), reader.end_pos());
  }
}


ExceptionExpectation ParseExpectation(int argc, const char* argv[]) {
  ExceptionExpectation expects;

  // Parse exception expectations from (the remainder of) the command line.
  int arg_index = 0;
  // Skip any flags.
  while (argc > arg_index && IsFlag(argv[arg_index])) arg_index++;
  if (argc > arg_index) {
    if (strncmp("throws", argv[arg_index], 7)) {
      // First argument after filename, if present, must be the verbatim
      // "throws", marking that the preparsing should fail with an exception.
      fail(NULL, "ERROR: Extra arguments not prefixed by \"throws\".\n");
    }
    expects.throws = true;
    do {
      arg_index++;
    } while (argc > arg_index && IsFlag(argv[arg_index]));
    if (argc > arg_index) {
      // Next argument is the exception type identifier.
      expects.type = argv[arg_index];
      do {
        arg_index++;
      } while (argc > arg_index && IsFlag(argv[arg_index]));
      if (argc > arg_index) {
        expects.beg_pos = atoi(argv[arg_index]);  // NOLINT
        do {
          arg_index++;
        } while (argc > arg_index && IsFlag(argv[arg_index]));
        if (argc > arg_index) {
          expects.end_pos = atoi(argv[arg_index]);  // NOLINT
        }
      }
    }
  }
  return expects;
}


int main(int argc, const char* argv[]) {
  // Parse command line.
  // Format:  preparser (<scriptfile> | -e "<source>")
  //                    ["throws" [<exn-type> [<start> [<end>]]]]
  // Any flags (except an initial -s) are ignored.

  // Check for mandatory filename argument.
  int arg_index = 1;
  if (argc <= arg_index) {
    fail(NULL, "ERROR: No filename on command line.\n");
  }
  const uint8_t* source = NULL;
  const char* filename = argv[arg_index];
  if (!strcmp(filename, "-e")) {
    arg_index++;
    if (argc <= arg_index) {
      fail(NULL, "ERROR: No source after -e on command line.\n");
    }
    source = reinterpret_cast<const uint8_t*>(argv[arg_index]);
  }
  // Check remainder of command line for exception expectations.
  arg_index++;
  ExceptionExpectation expects =
      ParseExpectation(argc - arg_index, argv + arg_index);

  ScopedPointer<uint8_t> buffer;
  size_t length;

  if (source == NULL) {
    // Open JS file.
    FILE* input = fopen(filename, "rb");
    if (input == NULL) {
      perror("ERROR: Error opening file");
      fflush(stderr);
      return EXIT_FAILURE;
    }
    // Find length of JS file.
    if (fseek(input, 0, SEEK_END) != 0) {
      perror("ERROR: Error during seek");
      fflush(stderr);
      return EXIT_FAILURE;
    }
    length = static_cast<size_t>(ftell(input));
    rewind(input);
    // Read JS file into memory buffer.
    buffer = new uint8_t[length];
    if (!ReadBuffer(input, *buffer, length)) {
      perror("ERROR: Reading file");
      fflush(stderr);
      return EXIT_FAILURE;
    }
    fclose(input);
    source = *buffer;
  } else {
    length = strlen(reinterpret_cast<const char*>(source));
  }

  // Preparse input file.
  AsciiInputStream input_buffer(source, length);
  size_t kMaxStackSize = 64 * 1024 * sizeof(void*);  // NOLINT
  v8::PreParserData data = v8::Preparse(&input_buffer, kMaxStackSize);

  // Fail if stack overflow.
  if (data.stack_overflow()) {
    fail(&data, "ERROR: Stack overflow\n");
  }

  // Check that the expected exception is thrown, if an exception is
  // expected.
  CheckException(&data, &expects);

  return EXIT_SUCCESS;
}
