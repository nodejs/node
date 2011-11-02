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

#ifdef _MSC_VER
#define V8_WIN32_LEAN_AND_MEAN
#include "win32-headers.h"
#endif

#include "../include/v8-preparser.h"

#include "globals.h"
#include "checks.h"
#include "allocation.h"
#include "utils.h"
#include "list.h"
#include "hashmap.h"
#include "preparse-data-format.h"
#include "preparse-data.h"
#include "preparser.h"

namespace v8 {
namespace internal {

// UTF16Buffer based on a v8::UnicodeInputStream.
class InputStreamUTF16Buffer : public UC16CharacterStream {
 public:
  /* The InputStreamUTF16Buffer maintains an internal buffer
   * that is filled in chunks from the UC16CharacterStream.
   * It also maintains unlimited pushback capability, but optimized
   * for small pushbacks.
   * The pushback_buffer_ pointer points to the limit of pushbacks
   * in the current buffer. There is room for a few pushback'ed chars before
   * the buffer containing the most recently read chunk. If this is overflowed,
   * an external buffer is allocated/reused to hold further pushbacks, and
   * pushback_buffer_ and buffer_cursor_/buffer_end_ now points to the
   * new buffer. When this buffer is read to the end again, the cursor is
   * switched back to the internal buffer
   */
  explicit InputStreamUTF16Buffer(v8::UnicodeInputStream* stream)
      : UC16CharacterStream(),
        stream_(stream),
        pushback_buffer_(buffer_),
        pushback_buffer_end_cache_(NULL),
        pushback_buffer_backing_(NULL),
        pushback_buffer_backing_size_(0) {
    buffer_cursor_ = buffer_end_ = buffer_ + kPushBackSize;
  }

  virtual ~InputStreamUTF16Buffer() {
    if (pushback_buffer_backing_ != NULL) {
      DeleteArray(pushback_buffer_backing_);
    }
  }

  virtual void PushBack(uc32 ch) {
    ASSERT(pos_ > 0);
    if (ch == kEndOfInput) {
      pos_--;
      return;
    }
    if (buffer_cursor_ <= pushback_buffer_) {
      // No more room in the current buffer to do pushbacks.
      if (pushback_buffer_end_cache_ == NULL) {
        // We have overflowed the pushback space at the beginning of buffer_.
        // Switch to using a separate allocated pushback buffer.
        if (pushback_buffer_backing_ == NULL) {
          // Allocate a buffer the first time we need it.
          pushback_buffer_backing_ = NewArray<uc16>(kPushBackSize);
          pushback_buffer_backing_size_ = kPushBackSize;
        }
        pushback_buffer_ = pushback_buffer_backing_;
        pushback_buffer_end_cache_ = buffer_end_;
        buffer_end_ = pushback_buffer_backing_ + pushback_buffer_backing_size_;
        buffer_cursor_ = buffer_end_ - 1;
      } else {
        // Hit the bottom of the allocated pushback buffer.
        // Double the buffer and continue.
        uc16* new_buffer = NewArray<uc16>(pushback_buffer_backing_size_ * 2);
        memcpy(new_buffer + pushback_buffer_backing_size_,
               pushback_buffer_backing_,
               pushback_buffer_backing_size_);
        DeleteArray(pushback_buffer_backing_);
        buffer_cursor_ = new_buffer + pushback_buffer_backing_size_;
        pushback_buffer_backing_ = pushback_buffer_ = new_buffer;
        buffer_end_ = pushback_buffer_backing_ + pushback_buffer_backing_size_;
      }
    }
    pushback_buffer_[buffer_cursor_ - pushback_buffer_- 1] =
        static_cast<uc16>(ch);
    pos_--;
  }

 protected:
  virtual bool ReadBlock() {
    if (pushback_buffer_end_cache_ != NULL) {
      buffer_cursor_ = buffer_;
      buffer_end_ = pushback_buffer_end_cache_;
      pushback_buffer_end_cache_ = NULL;
      return buffer_end_ > buffer_cursor_;
    }
    // Copy the top of the buffer into the pushback area.
    int32_t value;
    uc16* buffer_start = buffer_ + kPushBackSize;
    buffer_cursor_ = buffer_end_ = buffer_start;
    while ((value = stream_->Next()) >= 0) {
      if (value > static_cast<int32_t>(unibrow::Utf8::kMaxThreeByteChar)) {
        value = unibrow::Utf8::kBadChar;
      }
      // buffer_end_ is a const pointer, but buffer_ is writable.
      buffer_start[buffer_end_++ - buffer_start] = static_cast<uc16>(value);
      if (buffer_end_ == buffer_ + kPushBackSize + kBufferSize) break;
    }
    return buffer_end_ > buffer_start;
  }

  virtual unsigned SlowSeekForward(unsigned pos) {
    // Seeking in the input is not used by preparsing.
    // It's only used by the real parser based on preparser data.
    UNIMPLEMENTED();
    return 0;
  }

 private:
  static const unsigned kBufferSize = 512;
  static const unsigned kPushBackSize = 16;
  v8::UnicodeInputStream* const stream_;
  // Buffer holding first kPushBackSize characters of pushback buffer,
  // then kBufferSize chars of read-ahead.
  // The pushback buffer is only used if pushing back characters past
  // the start of a block.
  uc16 buffer_[kPushBackSize + kBufferSize];
  // Limit of pushbacks before new allocation is necessary.
  uc16* pushback_buffer_;
  // Only if that pushback buffer at the start of buffer_ isn't sufficient
  // is the following used.
  const uc16* pushback_buffer_end_cache_;
  uc16* pushback_buffer_backing_;
  unsigned pushback_buffer_backing_size_;
};


// Functions declared by allocation.h and implemented in both api.cc (for v8)
// or here (for a stand-alone preparser).

void FatalProcessOutOfMemory(const char* reason) {
  V8_Fatal(__FILE__, __LINE__, reason);
}

bool EnableSlowAsserts() { return true; }

}  // namespace internal.


UnicodeInputStream::~UnicodeInputStream() { }


PreParserData Preparse(UnicodeInputStream* input, size_t max_stack) {
  internal::InputStreamUTF16Buffer buffer(input);
  uintptr_t stack_limit = reinterpret_cast<uintptr_t>(&buffer) - max_stack;
  internal::UnicodeCache unicode_cache;
  internal::JavaScriptScanner scanner(&unicode_cache);
  scanner.Initialize(&buffer);
  internal::CompleteParserRecorder recorder;
  preparser::PreParser::PreParseResult result =
      preparser::PreParser::PreParseProgram(&scanner,
                                            &recorder,
                                            true,
                                            stack_limit);
  if (result == preparser::PreParser::kPreParseStackOverflow) {
    return PreParserData::StackOverflow();
  }
  internal::Vector<unsigned> pre_data = recorder.ExtractData();
  size_t size = pre_data.length() * sizeof(pre_data[0]);
  unsigned char* data = reinterpret_cast<unsigned char*>(pre_data.start());
  return PreParserData(size, data);
}

}  // namespace v8.


// Used by ASSERT macros and other immediate exits.
extern "C" void V8_Fatal(const char* file, int line, const char* format, ...) {
  exit(EXIT_FAILURE);
}
