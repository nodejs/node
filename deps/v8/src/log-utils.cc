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


bool Log::is_stopped_ = false;
Log::WritePtr Log::Write = NULL;
FILE* Log::output_handle_ = NULL;
LogDynamicBuffer* Log::output_buffer_ = NULL;
// Must be the same message as in Logger::PauseProfiler.
const char* Log::kDynamicBufferSeal = "profiler,\"pause\"\n";
Mutex* Log::mutex_ = NULL;
char* Log::message_buffer_ = NULL;


void Log::Init() {
  mutex_ = OS::CreateMutex();
  message_buffer_ = NewArray<char>(kMessageBufferSize);
}


void Log::OpenStdout() {
  ASSERT(!IsEnabled());
  output_handle_ = stdout;
  Write = WriteToFile;
  Init();
}


void Log::OpenFile(const char* name) {
  ASSERT(!IsEnabled());
  output_handle_ = OS::FOpen(name, OS::LogFileOpenMode);
  Write = WriteToFile;
  Init();
}


void Log::OpenMemoryBuffer() {
  ASSERT(!IsEnabled());
  output_buffer_ = new LogDynamicBuffer(
      kDynamicBufferBlockSize, kMaxDynamicBufferSize,
      kDynamicBufferSeal, strlen(kDynamicBufferSeal));
  Write = WriteToMemory;
  Init();
}


void Log::Close() {
  if (Write == WriteToFile) {
    fclose(output_handle_);
    output_handle_ = NULL;
  } else if (Write == WriteToMemory) {
    delete output_buffer_;
    output_buffer_ = NULL;
  } else {
    ASSERT(Write == NULL);
  }
  Write = NULL;

  DeleteArray(message_buffer_);
  message_buffer_ = NULL;

  delete mutex_;
  mutex_ = NULL;

  is_stopped_ = false;
}


int Log::GetLogLines(int from_pos, char* dest_buf, int max_size) {
  if (Write != WriteToMemory) return 0;
  ASSERT(output_buffer_ != NULL);
  ASSERT(from_pos >= 0);
  ASSERT(max_size >= 0);
  int actual_size = output_buffer_->Read(from_pos, dest_buf, max_size);
  ASSERT(actual_size <= max_size);
  if (actual_size == 0) return 0;

  // Find previous log line boundary.
  char* end_pos = dest_buf + actual_size - 1;
  while (end_pos >= dest_buf && *end_pos != '\n') --end_pos;
  actual_size = end_pos - dest_buf + 1;
  ASSERT(actual_size <= max_size);
  return actual_size;
}


LogMessageBuilder::WriteFailureHandler
    LogMessageBuilder::write_failure_handler = NULL;


LogMessageBuilder::LogMessageBuilder(): sl(Log::mutex_), pos_(0) {
  ASSERT(Log::message_buffer_ != NULL);
}


void LogMessageBuilder::Append(const char* format, ...) {
  Vector<char> buf(Log::message_buffer_ + pos_,
                   Log::kMessageBufferSize - pos_);
  va_list args;
  va_start(args, format);
  AppendVA(format, args);
  va_end(args);
  ASSERT(pos_ <= Log::kMessageBufferSize);
}


void LogMessageBuilder::AppendVA(const char* format, va_list args) {
  Vector<char> buf(Log::message_buffer_ + pos_,
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
    Log::message_buffer_[pos_++] = c;
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
  static Address last_address_ = NULL;
  AppendAddress(addr, last_address_);
  last_address_ = addr;
}


void LogMessageBuilder::AppendAddress(Address addr, Address bias) {
  if (!FLAG_compress_log) {
    Append("0x%" V8PRIxPTR, addr);
  } else if (bias == NULL) {
    Append("%" V8PRIxPTR, addr);
  } else {
    uintptr_t delta;
    char sign;
    if (addr >= bias) {
      delta = addr - bias;
      sign = '+';
    } else {
      delta = bias - addr;
      sign = '-';
    }
    Append("%c%" V8PRIxPTR, sign, delta);
  }
}


void LogMessageBuilder::AppendDetailed(String* str, bool show_impl_info) {
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
  Vector<char> buf(Log::message_buffer_ + pos_,
                   Log::kMessageBufferSize - pos_);
  OS::StrNCpy(buf, str, len);
  pos_ += len;
  ASSERT(pos_ <= Log::kMessageBufferSize);
}


bool LogMessageBuilder::StoreInCompressor(LogRecordCompressor* compressor) {
  return compressor->Store(Vector<const char>(Log::message_buffer_, pos_));
}


bool LogMessageBuilder::RetrieveCompressedPrevious(
    LogRecordCompressor* compressor, const char* prefix) {
  pos_ = 0;
  if (prefix[0] != '\0') Append(prefix);
  Vector<char> prev_record(Log::message_buffer_ + pos_,
                           Log::kMessageBufferSize - pos_);
  const bool has_prev = compressor->RetrievePreviousCompressed(&prev_record);
  if (!has_prev) return false;
  pos_ += prev_record.length();
  return true;
}


void LogMessageBuilder::WriteToLogFile() {
  ASSERT(pos_ <= Log::kMessageBufferSize);
  const int written = Log::Write(Log::message_buffer_, pos_);
  if (written != pos_ && write_failure_handler != NULL) {
    write_failure_handler();
  }
}


void LogMessageBuilder::WriteCStringToLogFile(const char* str) {
  const int len = strlen(str);
  const int written = Log::Write(str, len);
  if (written != len && write_failure_handler != NULL) {
    write_failure_handler();
  }
}


// Formatting string for back references to the whole line. E.g. "#2" means
// "the second line above".
const char* LogRecordCompressor::kLineBackwardReferenceFormat = "#%d";

// Formatting string for back references. E.g. "#2:10" means
// "the second line above, start from char 10 (0-based)".
const char* LogRecordCompressor::kBackwardReferenceFormat = "#%d:%d";


LogRecordCompressor::~LogRecordCompressor() {
  for (int i = 0; i < buffer_.length(); ++i) {
    buffer_[i].Dispose();
  }
}


static int GetNumberLength(int number) {
  ASSERT(number >= 0);
  ASSERT(number < 10000);
  if (number < 10) return 1;
  if (number < 100) return 2;
  if (number < 1000) return 3;
  return 4;
}


int LogRecordCompressor::GetBackwardReferenceSize(int distance, int pos) {
  // See kLineBackwardReferenceFormat and kBackwardReferenceFormat.
  return pos == 0 ? GetNumberLength(distance) + 1
      : GetNumberLength(distance) + GetNumberLength(pos) + 2;
}


void LogRecordCompressor::PrintBackwardReference(Vector<char> dest,
                                                 int distance,
                                                 int pos) {
  if (pos == 0) {
    OS::SNPrintF(dest, kLineBackwardReferenceFormat, distance);
  } else {
    OS::SNPrintF(dest, kBackwardReferenceFormat, distance, pos);
  }
}


bool LogRecordCompressor::Store(const Vector<const char>& record) {
  // Check if the record is the same as the last stored one.
  if (curr_ != -1) {
    Vector<const char>& curr = buffer_[curr_];
    if (record.length() == curr.length()
        && strncmp(record.start(), curr.start(), record.length()) == 0) {
      return false;
    }
  }
  // buffer_ is circular.
  prev_ = curr_++;
  curr_ %= buffer_.length();
  Vector<char> record_copy = Vector<char>::New(record.length());
  memcpy(record_copy.start(), record.start(), record.length());
  buffer_[curr_].Dispose();
  buffer_[curr_] =
      Vector<const char>(record_copy.start(), record_copy.length());
  return true;
}


bool LogRecordCompressor::RetrievePreviousCompressed(
    Vector<char>* prev_record) {
  if (prev_ == -1) return false;

  int index = prev_;
  // Distance from prev_.
  int distance = 0;
  // Best compression result among records in the buffer.
  struct {
    intptr_t truncated_len;
    int distance;
    int copy_from_pos;
    int backref_size;
  } best = {-1, 0, 0, 0};
  Vector<const char>& prev = buffer_[prev_];
  const char* const prev_start = prev.start();
  const char* const prev_end = prev.start() + prev.length();
  do {
    // We're moving backwards until we reach the current record.
    // Remember that buffer_ is circular.
    if (--index == -1) index = buffer_.length() - 1;
    ++distance;
    if (index == curr_) break;

    Vector<const char>& data = buffer_[index];
    if (data.start() == NULL) break;
    const char* const data_end = data.start() + data.length();
    const char* prev_ptr = prev_end;
    const char* data_ptr = data_end;
    // Compare strings backwards, stop on the last matching character.
    while (prev_ptr != prev_start && data_ptr != data.start()
          && *(prev_ptr - 1) == *(data_ptr - 1)) {
      --prev_ptr;
      --data_ptr;
    }
    const intptr_t truncated_len = prev_end - prev_ptr;
    const int copy_from_pos = data_ptr - data.start();
    // Check if the length of compressed tail is enough.
    if (truncated_len <= kMaxBackwardReferenceSize
        && truncated_len <= GetBackwardReferenceSize(distance, copy_from_pos)) {
      continue;
    }

    // Record compression results.
    if (truncated_len > best.truncated_len) {
      best.truncated_len = truncated_len;
      best.distance = distance;
      best.copy_from_pos = copy_from_pos;
      best.backref_size = GetBackwardReferenceSize(distance, copy_from_pos);
    }
  } while (true);

  if (best.distance == 0) {
    // Can't compress the previous record. Return as is.
    ASSERT(prev_record->length() >= prev.length());
    memcpy(prev_record->start(), prev.start(), prev.length());
    prev_record->Truncate(prev.length());
  } else {
    // Copy the uncompressible part unchanged.
    const intptr_t unchanged_len = prev.length() - best.truncated_len;
    // + 1 for '\0'.
    ASSERT(prev_record->length() >= unchanged_len + best.backref_size + 1);
    memcpy(prev_record->start(), prev.start(), unchanged_len);
    // Append the backward reference.
    Vector<char> backref(
        prev_record->start() + unchanged_len, best.backref_size + 1);
    PrintBackwardReference(backref, best.distance, best.copy_from_pos);
    ASSERT(strlen(backref.start()) - best.backref_size == 0);
    prev_record->Truncate(unchanged_len + best.backref_size);
  }
  return true;
}

#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal
