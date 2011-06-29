// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "log-utils.h"
#include "string-stream.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_LOGGING_AND_PROFILING

LogDynamicBuffer::LogDynamicBuffer(
    int block_size, int max_size, const char* seal, int seal_size)
    : block_size_(block_size),
      max_size_(max_size - (max_size % block_size_)),
      seal_(seal),
      seal_size_(seal_size),
      blocks_(max_size_ / block_size_ + 1),
      write_pos_(0), block_index_(0), block_write_pos_(0), is_sealed_(false) {
  ASSERT(BlocksCount() > 0);
  AllocateBlock(0);
  for (int i = 1; i < BlocksCount(); ++i) {
    blocks_[i] = NULL;
  }
}


LogDynamicBuffer::~LogDynamicBuffer() {
  for (int i = 0; i < BlocksCount(); ++i) {
    DeleteArray(blocks_[i]);
  }
}


int LogDynamicBuffer::Read(int from_pos, char* dest_buf, int buf_size) {
  if (buf_size == 0) return 0;
  int read_pos = from_pos;
  int block_read_index = BlockIndex(from_pos);
  int block_read_pos = PosInBlock(from_pos);
  int dest_buf_pos = 0;
  // Read until dest_buf is filled, or write_pos_ encountered.
  while (read_pos < write_pos_ && dest_buf_pos < buf_size) {
    const int read_size = Min(write_pos_ - read_pos,
        Min(buf_size - dest_buf_pos, block_size_ - block_read_pos));
    memcpy(dest_buf + dest_buf_pos,
           blocks_[block_read_index] + block_read_pos, read_size);
    block_read_pos += read_size;
    dest_buf_pos += read_size;
    read_pos += read_size;
    if (block_read_pos == block_size_) {
      block_read_pos = 0;
      ++block_read_index;
    }
  }
  return dest_buf_pos;
}


int LogDynamicBuffer::Seal() {
  WriteInternal(seal_, seal_size_);
  is_sealed_ = true;
  return 0;
}


int LogDynamicBuffer::Write(const char* data, int data_size) {
  if (is_sealed_) {
    return 0;
  }
  if ((write_pos_ + data_size) <= (max_size_ - seal_size_)) {
    return WriteInternal(data, data_size);
  } else {
    return Seal();
  }
}


int LogDynamicBuffer::WriteInternal(const char* data, int data_size) {
  int data_pos = 0;
  while (data_pos < data_size) {
    const int write_size =
        Min(data_size - data_pos, block_size_ - block_write_pos_);
    memcpy(blocks_[block_index_] + block_write_pos_, data + data_pos,
           write_size);
    block_write_pos_ += write_size;
    data_pos += write_size;
    if (block_write_pos_ == block_size_) {
      block_write_pos_ = 0;
      AllocateBlock(++block_index_);
    }
  }
  write_pos_ += data_size;
  return data_size;
}

// Must be the same message as in Logger::PauseProfiler.
const char* const Log::kDynamicBufferSeal = "profiler,\"pause\"\n";

Log::Log(Logger* logger)
  : write_to_file_(false),
    is_stopped_(false),
    output_handle_(NULL),
    ll_output_handle_(NULL),
    output_buffer_(NULL),
    mutex_(NULL),
    message_buffer_(NULL),
    logger_(logger) {
}


static void AddIsolateIdIfNeeded(StringStream* stream) {
  Isolate* isolate = Isolate::Current();
  if (isolate->IsDefaultIsolate()) return;
  stream->Add("isolate-%p-", isolate);
}


void Log::Initialize() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  mutex_ = OS::CreateMutex();
  message_buffer_ = NewArray<char>(kMessageBufferSize);

  // --log-all enables all the log flags.
  if (FLAG_log_all) {
    FLAG_log_runtime = true;
    FLAG_log_api = true;
    FLAG_log_code = true;
    FLAG_log_gc = true;
    FLAG_log_suspect = true;
    FLAG_log_handles = true;
    FLAG_log_regexp = true;
  }

  // --prof implies --log-code.
  if (FLAG_prof) FLAG_log_code = true;

  // --prof_lazy controls --log-code, implies --noprof_auto.
  if (FLAG_prof_lazy) {
    FLAG_log_code = false;
    FLAG_prof_auto = false;
  }

  bool start_logging = FLAG_log || FLAG_log_runtime || FLAG_log_api
      || FLAG_log_code || FLAG_log_gc || FLAG_log_handles || FLAG_log_suspect
      || FLAG_log_regexp || FLAG_log_state_changes || FLAG_ll_prof;

  bool open_log_file = start_logging || FLAG_prof_lazy;

  // If we're logging anything, we need to open the log file.
  if (open_log_file) {
    if (strcmp(FLAG_logfile, "-") == 0) {
      OpenStdout();
    } else if (strcmp(FLAG_logfile, "*") == 0) {
      OpenMemoryBuffer();
    } else  {
      if (strchr(FLAG_logfile, '%') != NULL ||
          !Isolate::Current()->IsDefaultIsolate()) {
        // If there's a '%' in the log file name we have to expand
        // placeholders.
        HeapStringAllocator allocator;
        StringStream stream(&allocator);
        AddIsolateIdIfNeeded(&stream);
        for (const char* p = FLAG_logfile; *p; p++) {
          if (*p == '%') {
            p++;
            switch (*p) {
              case '\0':
                // If there's a % at the end of the string we back up
                // one character so we can escape the loop properly.
                p--;
                break;
              case 't': {
                // %t expands to the current time in milliseconds.
                double time = OS::TimeCurrentMillis();
                stream.Add("%.0f", FmtElm(time));
                break;
              }
              case '%':
                // %% expands (contracts really) to %.
                stream.Put('%');
                break;
              default:
                // All other %'s expand to themselves.
                stream.Put('%');
                stream.Put(*p);
                break;
            }
          } else {
            stream.Put(*p);
          }
        }
        SmartPointer<const char> expanded = stream.ToCString();
        OpenFile(*expanded);
      } else {
        OpenFile(FLAG_logfile);
      }
    }
  }
#endif
}


void Log::OpenStdout() {
  ASSERT(!IsEnabled());
  output_handle_ = stdout;
  write_to_file_ = true;
}


// Extension added to V8 log file name to get the low-level log name.
static const char kLowLevelLogExt[] = ".ll";

// File buffer size of the low-level log. We don't use the default to
// minimize the associated overhead.
static const int kLowLevelLogBufferSize = 2 * MB;


void Log::OpenFile(const char* name) {
  ASSERT(!IsEnabled());
  output_handle_ = OS::FOpen(name, OS::LogFileOpenMode);
  write_to_file_ = true;
  if (FLAG_ll_prof) {
    // Open the low-level log file.
    size_t len = strlen(name);
    ScopedVector<char> ll_name(static_cast<int>(len + sizeof(kLowLevelLogExt)));
    memcpy(ll_name.start(), name, len);
    memcpy(ll_name.start() + len, kLowLevelLogExt, sizeof(kLowLevelLogExt));
    ll_output_handle_ = OS::FOpen(ll_name.start(), OS::LogFileOpenMode);
    setvbuf(ll_output_handle_, NULL, _IOFBF, kLowLevelLogBufferSize);
  }
}


void Log::OpenMemoryBuffer() {
  ASSERT(!IsEnabled());
  output_buffer_ = new LogDynamicBuffer(
      kDynamicBufferBlockSize, kMaxDynamicBufferSize,
      kDynamicBufferSeal, StrLength(kDynamicBufferSeal));
  write_to_file_ = false;
}


void Log::Close() {
  if (write_to_file_) {
    if (output_handle_ != NULL) fclose(output_handle_);
    output_handle_ = NULL;
    if (ll_output_handle_ != NULL) fclose(ll_output_handle_);
    ll_output_handle_ = NULL;
  } else {
    delete output_buffer_;
    output_buffer_ = NULL;
  }

  DeleteArray(message_buffer_);
  message_buffer_ = NULL;

  delete mutex_;
  mutex_ = NULL;

  is_stopped_ = false;
}


int Log::GetLogLines(int from_pos, char* dest_buf, int max_size) {
  if (write_to_file_) return 0;
  ASSERT(output_buffer_ != NULL);
  ASSERT(from_pos >= 0);
  ASSERT(max_size >= 0);
  int actual_size = output_buffer_->Read(from_pos, dest_buf, max_size);
  ASSERT(actual_size <= max_size);
  if (actual_size == 0) return 0;

  // Find previous log line boundary.
  char* end_pos = dest_buf + actual_size - 1;
  while (end_pos >= dest_buf && *end_pos != '\n') --end_pos;
  actual_size = static_cast<int>(end_pos - dest_buf + 1);
  // If the assertion below is hit, it means that there was no line end
  // found --- something wrong has happened.
  ASSERT(actual_size > 0);
  ASSERT(actual_size <= max_size);
  return actual_size;
}


LogMessageBuilder::LogMessageBuilder(Logger* logger)
  : log_(logger->log_),
    sl(log_->mutex_),
    pos_(0) {
  ASSERT(log_->message_buffer_ != NULL);
}


void LogMessageBuilder::Append(const char* format, ...) {
  Vector<char> buf(log_->message_buffer_ + pos_,
                   Log::kMessageBufferSize - pos_);
  va_list args;
  va_start(args, format);
  AppendVA(format, args);
  va_end(args);
  ASSERT(pos_ <= Log::kMessageBufferSize);
}


void LogMessageBuilder::AppendVA(const char* format, va_list args) {
  Vector<char> buf(log_->message_buffer_ + pos_,
                   Log::kMessageBufferSize - pos_);
  int result = v8::internal::OS::VSNPrintF(buf, format, args);

  // Result is -1 if output was truncated.
  if (result >= 0) {
    pos_ += result;
  } else {
    pos_ = Log::kMessageBufferSize;
  }
  ASSERT(pos_ <= Log::kMessageBufferSize);
}


void LogMessageBuilder::Append(const char c) {
  if (pos_ < Log::kMessageBufferSize) {
    log_->message_buffer_[pos_++] = c;
  }
  ASSERT(pos_ <= Log::kMessageBufferSize);
}


void LogMessageBuilder::Append(String* str) {
  AssertNoAllocation no_heap_allocation;  // Ensure string stay valid.
  int length = str->length();
  for (int i = 0; i < length; i++) {
    Append(static_cast<char>(str->Get(i)));
  }
}


void LogMessageBuilder::AppendAddress(Address addr) {
  Append("0x%" V8PRIxPTR, addr);
}


void LogMessageBuilder::AppendDetailed(String* str, bool show_impl_info) {
  if (str == NULL) return;
  AssertNoAllocation no_heap_allocation;  // Ensure string stay valid.
  int len = str->length();
  if (len > 0x1000)
    len = 0x1000;
  if (show_impl_info) {
    Append(str->IsAsciiRepresentation() ? 'a' : '2');
    if (StringShape(str).IsExternal())
      Append('e');
    if (StringShape(str).IsSymbol())
      Append('#');
    Append(":%i:", str->length());
  }
  for (int i = 0; i < len; i++) {
    uc32 c = str->Get(i);
    if (c > 0xff) {
      Append("\\u%04x", c);
    } else if (c < 32 || c > 126) {
      Append("\\x%02x", c);
    } else if (c == ',') {
      Append("\\,");
    } else if (c == '\\') {
      Append("\\\\");
    } else if (c == '\"') {
      Append("\"\"");
    } else {
      Append("%lc", c);
    }
  }
}


void LogMessageBuilder::AppendStringPart(const char* str, int len) {
  if (pos_ + len > Log::kMessageBufferSize) {
    len = Log::kMessageBufferSize - pos_;
    ASSERT(len >= 0);
    if (len == 0) return;
  }
  Vector<char> buf(log_->message_buffer_ + pos_,
                   Log::kMessageBufferSize - pos_);
  OS::StrNCpy(buf, str, len);
  pos_ += len;
  ASSERT(pos_ <= Log::kMessageBufferSize);
}


void LogMessageBuilder::WriteToLogFile() {
  ASSERT(pos_ <= Log::kMessageBufferSize);
  const int written = log_->write_to_file_ ?
      log_->WriteToFile(log_->message_buffer_, pos_) :
      log_->WriteToMemory(log_->message_buffer_, pos_);
  if (written != pos_) {
    log_->stop();
    log_->logger_->LogFailure();
  }
}


#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal
