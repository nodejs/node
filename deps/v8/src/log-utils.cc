// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/log-utils.h"

#include "src/assert-scope.h"
#include "src/base/platform/platform.h"
#include "src/objects-inl.h"
#include "src/string-stream.h"
#include "src/utils.h"
#include "src/vector.h"
#include "src/version.h"

namespace v8 {
namespace internal {


const char* const Log::kLogToTemporaryFile = "&";
const char* const Log::kLogToConsole = "-";

// static
FILE* Log::CreateOutputHandle(const char* file_name) {
  // If we're logging anything, we need to open the log file.
  if (!Log::InitLogAtStart()) {
    return nullptr;
  } else if (strcmp(file_name, kLogToConsole) == 0) {
    return stdout;
  } else if (strcmp(file_name, kLogToTemporaryFile) == 0) {
    return base::OS::OpenTemporaryFile();
  } else {
    return base::OS::FOpen(file_name, base::OS::LogFileOpenMode);
  }
}

Log::Log(Logger* logger, const char* file_name)
    : is_stopped_(false),
      output_handle_(Log::CreateOutputHandle(file_name)),
      os_(output_handle_ == nullptr ? stdout : output_handle_),
      format_buffer_(NewArray<char>(kMessageBufferSize)),
      logger_(logger) {
  // --log-all enables all the log flags.
  if (FLAG_log_all) {
    FLAG_log_api = true;
    FLAG_log_code = true;
    FLAG_log_suspect = true;
    FLAG_log_handles = true;
    FLAG_log_internal_timer_events = true;
    FLAG_log_function_events = true;
  }

  // --prof implies --log-code.
  if (FLAG_prof) FLAG_log_code = true;

  if (output_handle_ == nullptr) return;
  Log::MessageBuilder msg(this);
  LogSeparator kNext = LogSeparator::kSeparator;
  msg << "v8-version" << kNext << Version::GetMajor() << kNext
      << Version::GetMinor() << kNext << Version::GetBuild() << kNext
      << Version::GetPatch();
  if (strlen(Version::GetEmbedder()) != 0) {
    msg << kNext << Version::GetEmbedder();
  }
  msg << kNext << Version::IsCandidate();
  msg.WriteToLogFile();
}

FILE* Log::Close() {
  FILE* result = nullptr;
  if (output_handle_ != nullptr) {
    if (strcmp(FLAG_logfile, kLogToTemporaryFile) != 0) {
      fclose(output_handle_);
    } else {
      result = output_handle_;
    }
  }
  output_handle_ = nullptr;

  DeleteArray(format_buffer_);
  format_buffer_ = nullptr;

  is_stopped_ = false;
  return result;
}

Log::MessageBuilder::MessageBuilder(Log* log)
    : log_(log), lock_guard_(&log_->mutex_) {
  DCHECK_NOT_NULL(log_->format_buffer_);
}

void Log::MessageBuilder::AppendString(String* str,
                                       base::Optional<int> length_limit) {
  if (str == nullptr) return;

  DisallowHeapAllocation no_gc;  // Ensure string stays valid.
  int length = str->length();
  if (length_limit) length = std::min(length, *length_limit);
  for (int i = 0; i < length; i++) {
    uint16_t c = str->Get(i);
    if (c <= 0xFF) {
      AppendCharacter(static_cast<char>(c));
    } else {
      // Escape non-ascii characters.
      AppendRawFormatString("\\u%04x", c & 0xFFFF);
    }
  }
}

void Log::MessageBuilder::AppendString(Vector<const char> str) {
  for (auto i = str.begin(); i < str.end(); i++) AppendCharacter(*i);
}

void Log::MessageBuilder::AppendString(const char* str) {
  if (str == nullptr) return;
  AppendString(str, strlen(str));
}

void Log::MessageBuilder::AppendString(const char* str, size_t length) {
  if (str == nullptr) return;

  for (size_t i = 0; i < length; i++) {
    DCHECK_NE(str[i], '\0');
    AppendCharacter(str[i]);
  }
}

void Log::MessageBuilder::AppendFormatString(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int length = FormatStringIntoBuffer(format, args);
  va_end(args);
  for (int i = 0; i < length; i++) {
    DCHECK_NE(log_->format_buffer_[i], '\0');
    AppendCharacter(log_->format_buffer_[i]);
  }
}

void Log::MessageBuilder::AppendCharacter(char c) {
  if (c >= 32 && c <= 126) {
    if (c == ',') {
      // Escape commas to avoid adding column separators.
      AppendRawFormatString("\\x2C");
    } else if (c == '\\') {
      AppendRawFormatString("\\\\");
    } else {
      // Safe, printable ascii character.
      AppendRawCharacter(c);
    }
  } else if (c == '\n') {
    // Escape newlines to avoid adding row separators.
    AppendRawFormatString("\\n");
  } else {
    // Escape non-printable characters.
    AppendRawFormatString("\\x%02x", c & 0xFF);
  }
}

void Log::MessageBuilder::AppendSymbolName(Symbol* symbol) {
  DCHECK(symbol);
  OFStream& os = log_->os_;
  os << "symbol(";
  if (!symbol->name()->IsUndefined()) {
    os << "\"";
    AppendSymbolNameDetails(String::cast(symbol->name()), false);
    os << "\" ";
  }
  os << "hash " << std::hex << symbol->Hash() << std::dec << ")";
}

void Log::MessageBuilder::AppendSymbolNameDetails(String* str,
                                                  bool show_impl_info) {
  if (str == nullptr) return;

  DisallowHeapAllocation no_gc;  // Ensure string stays valid.
  OFStream& os = log_->os_;
  int limit = str->length();
  if (limit > 0x1000) limit = 0x1000;
  if (show_impl_info) {
    os << (str->IsOneByteRepresentation() ? 'a' : '2');
    if (StringShape(str).IsExternal()) os << 'e';
    if (StringShape(str).IsInternalized()) os << '#';
    os << ':' << str->length() << ':';
  }
  AppendString(str, limit);
}

int Log::MessageBuilder::FormatStringIntoBuffer(const char* format,
                                                va_list args) {
  Vector<char> buf(log_->format_buffer_, Log::kMessageBufferSize);
  int length = v8::internal::VSNPrintF(buf, format, args);
  // |length| is -1 if output was truncated.
  if (length == -1) length = Log::kMessageBufferSize;
  DCHECK_LE(length, Log::kMessageBufferSize);
  DCHECK_GE(length, 0);
  return length;
}

void Log::MessageBuilder::AppendRawFormatString(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int length = FormatStringIntoBuffer(format, args);
  va_end(args);
  for (int i = 0; i < length; i++) {
    DCHECK_NE(log_->format_buffer_[i], '\0');
    AppendRawCharacter(log_->format_buffer_[i]);
  }
}

void Log::MessageBuilder::AppendRawCharacter(char c) { log_->os_ << c; }

void Log::MessageBuilder::WriteToLogFile() { log_->os_ << std::endl; }

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<const char*>(
    const char* string) {
  this->AppendString(string);
  return *this;
}

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<void*>(void* pointer) {
  OFStream& os = log_->os_;
  // Manually format the pointer since on Windows we do not consistently
  // get a "0x" prefix.
  os << "0x" << std::hex << reinterpret_cast<intptr_t>(pointer) << std::dec;
  return *this;
}

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<char>(char c) {
  this->AppendCharacter(c);
  return *this;
}

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<String*>(String* string) {
  this->AppendString(string);
  return *this;
}

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<Symbol*>(Symbol* symbol) {
  this->AppendSymbolName(symbol);
  return *this;
}

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<Name*>(Name* name) {
  if (name->IsString()) {
    this->AppendString(String::cast(name));
  } else {
    this->AppendSymbolName(Symbol::cast(name));
  }
  return *this;
}

template <>
Log::MessageBuilder& Log::MessageBuilder::operator<<<LogSeparator>(
    LogSeparator separator) {
  // Skip escaping to create a new column.
  this->AppendRawCharacter(',');
  return *this;
}

}  // namespace internal
}  // namespace v8
