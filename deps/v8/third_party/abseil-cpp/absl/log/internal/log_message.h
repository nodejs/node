// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: log/internal/log_message.h
// -----------------------------------------------------------------------------
//
// This file declares `class absl::log_internal::LogMessage`. This class more or
// less represents a particular log message. LOG/CHECK macros create a temporary
// instance of `LogMessage` and then stream values to it.  At the end of the
// LOG/CHECK statement, the LogMessage is voidified by operator&&, and `Flush()`
// directs the message to the registered log sinks.  Heap-allocation of
// `LogMessage` is unsupported.  Construction outside of a `LOG` macro is
// unsupported.

#ifndef ABSL_LOG_INTERNAL_LOG_MESSAGE_H_
#define ABSL_LOG_INTERNAL_LOG_MESSAGE_H_

#include <wchar.h>

#include <cstddef>
#include <ios>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/errno_saver.h"
#include "absl/base/log_severity.h"
#include "absl/base/nullability.h"
#include "absl/log/internal/nullguard.h"
#include "absl/log/internal/structured_proto.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/source_location.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {
constexpr int kLogMessageBufferSize = 15000;

enum class StructuredStringType;

class LogMessage {
 public:
  struct InfoTag {};
  struct WarningTag {};
  struct ErrorTag {};

  // Used for `LOG`.  Taking `const char *` instead of `string_view` keeps
  // callsites a little bit smaller at the cost of doing `strlen` at runtime.
  ABSL_ATTRIBUTE_COLD LogMessage(const char* absl_nonnull file, int line,
                                 absl::LogSeverity severity);
  // Used for FFI integrations that don't have a NUL-terminated string.
  ABSL_ATTRIBUTE_COLD LogMessage(absl::string_view file, int line,
                                 absl::LogSeverity severity);
  // These constructors are slightly smaller/faster to call; the severity is
  // curried into the function pointer.
  ABSL_ATTRIBUTE_COLD ABSL_ATTRIBUTE_NOINLINE
  LogMessage(const char* absl_nonnull file, int line, InfoTag);
  ABSL_ATTRIBUTE_COLD ABSL_ATTRIBUTE_NOINLINE
  LogMessage(const char* absl_nonnull file, int line, WarningTag);
  ABSL_ATTRIBUTE_COLD ABSL_ATTRIBUTE_NOINLINE
  LogMessage(const char* absl_nonnull file, int line, ErrorTag);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  ABSL_ATTRIBUTE_COLD ~LogMessage();

  // Methods that return `*this` are inline wrappers around out-of-line void
  // helpers so the compiler knows the returned reference aliases `this`.

  // Overrides the location inferred from the callsite.  The string pointed to
  // by `file` must be valid until the end of the statement.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& AtLocation(absl::string_view file,
                                                      int line) {
    AtLocationImpl(file, line);
    return *this;
  }
  // `loc` doesn't default to `absl::SourceLocation::current()` here since the
  // callsite is already the default location for `LOG` statements.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& AtLocation(
      absl::SourceLocation loc) {
    return AtLocation(loc.file_name(), static_cast<int>(loc.line()));
  }
  // Omits the prefix from this line.  The prefix includes metadata about the
  // logged data such as source code location and timestamp.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& NoPrefix() {
    NoPrefixImpl();
    return *this;
  }
  // Sets the verbosity field of the logged message as if it was logged by
  // `VLOG(verbose_level)`.  Unlike `VLOG`, this method does not affect
  // evaluation of the statement when the specified `verbose_level` has been
  // disabled.  The only effect is on `absl::LogSink` implementations which
  // make use of the `absl::LogSink::verbosity()` value.  The value
  // `absl::LogEntry::kNoVerbosityLevel` can be specified to mark the message
  // not verbose.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& WithVerbosity(int verbose_level) {
    WithVerbosityImpl(verbose_level);
    return *this;
  }
  // Uses the specified timestamp instead of one collected in the constructor.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& WithTimestamp(absl::Time timestamp) {
    WithTimestampImpl(timestamp);
    return *this;
  }
  // Uses the specified thread ID instead of one collected in the constructor.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& WithThreadID(
      absl::LogEntry::tid_t tid) {
    WithThreadIDImpl(tid);
    return *this;
  }
  // Copies all metadata (but no data) from the specified `absl::LogEntry`.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& WithMetadataFrom(
      const absl::LogEntry& entry) {
    WithMetadataFromImpl(entry);
    return *this;
  }
  // Appends to the logged message a colon, a space, a textual description of
  // the current value of `errno` (as by strerror(3)), and the numerical value
  // of `errno`.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& WithPerror() {
    WithPerrorImpl();
    return *this;
  }
  // Sends this message to `*sink` in addition to whatever other sinks it would
  // otherwise have been sent to.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& ToSinkAlso(
      absl::LogSink* absl_nonnull sink) {
    ToSinkAlsoImpl(sink);
    return *this;
  }
  // Sends this message to `*sink` and no others.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& ToSinkOnly(
      absl::LogSink* absl_nonnull sink) {
    ToSinkOnlyImpl(sink);
    return *this;
  }

  // Don't call this method from outside this library.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& InternalStream() { return *this; }

  // By-value overloads for small, common types let us overlook common failures
  // to define globals and static data members (i.e. in a .cc file).
  // Values are passed by value to `Stream` so they stay in registers.
  // NOLINTBEGIN(runtime/int)
  // NOLINTBEGIN(google-runtime-int)
  // clang-format off:  The CUDA toolchain cannot handle these <<<'s
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(char v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(signed char v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(unsigned char v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(signed short v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(signed int v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(signed long v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(signed long long v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(unsigned short v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(unsigned int v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(unsigned long v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(unsigned long long v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(void* absl_nullable v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(
      const void* absl_nullable v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(float v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(double v) {
    Stream(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(bool v) {
    Stream(v);
    return *this;
  }
  // clang-format on
  // NOLINTEND(google-runtime-int)
  // NOLINTEND(runtime/int)

  // These overloads are more efficient since no `ostream` is involved.
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(const std::string& v) {
    CopyToEncodedBuffer<StringType::kNotLiteral>(v);
    return *this;
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(absl::string_view v) {
    CopyToEncodedBuffer<StringType::kNotLiteral>(v);
    return *this;
  }

  // Wide string overloads (since std::ostream does not provide them).
  LogMessage& operator<<(const std::wstring& v);
  LogMessage& operator<<(std::wstring_view v);
  // `const wchar_t*` is handled by `operator<< <const wchar_t*>`.
  LogMessage& operator<<(wchar_t* absl_nullable v);
  LogMessage& operator<<(wchar_t v);

  // Overload for absl::SourceLocation or the std::source_location alias.
  LogMessage& operator<<(const absl::SourceLocation& loc) {
    OstreamView view(*data_);
    view.stream() << loc.file_name() << ':' << loc.line();
    return *this;
  }

  // Handle stream manipulators e.g. std::endl.
  LogMessage& operator<<(std::ostream& (*absl_nonnull m)(std::ostream& os));
  LogMessage& operator<<(std::ios_base& (*absl_nonnull m)(std::ios_base& os));

  // Literal strings.  This allows us to record C string literals as literals in
  // the logging.proto.Value.
  //
  // Allow this overload to be inlined to prevent generating instantiations of
  // this template for every value of `SIZE` encountered in each source code
  // file. That significantly increases linker input sizes. Inlining is cheap
  // because the argument to this overload is almost always a string literal so
  // the call to `strlen` can be replaced at compile time. The overloads for
  // `char[]`/`wchar_t[]` below should not be inlined. The compiler typically
  // does not have the string at compile time and cannot replace the call to
  // `strlen` so inlining it increases the binary size. See the discussion on
  // cl/107527369.
  template <int SIZE>
  LogMessage& operator<<(const char (&buf)[SIZE]);
  template <int SIZE>
  LogMessage& operator<<(const wchar_t (&buf)[SIZE]);

  // This prevents non-const `char[]` arrays from looking like literals.
  template <int SIZE>
  ABSL_ATTRIBUTE_NOINLINE LogMessage& operator<<(char (&buf)[SIZE]);
  // `wchar_t[SIZE]` is handled by `operator<< <const wchar_t*>`.

  // Types that support `AbslStringify()` are serialized that way.
  // Types that don't support `AbslStringify()` but do support streaming into a
  // `std::ostream&` are serialized that way.
  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE LogMessage& operator<<(const T& v);

  // Dispatches the completed `absl::LogEntry` to applicable `absl::LogSink`s.
  void Flush();

  // Note: We explicitly do not support `operator<<` for non-const references
  // because it breaks logging of non-integer bitfield types (i.e., enums).

 protected:
  // Call `abort()` or similar to perform `LOG(FATAL)` crash.  It is assumed
  // that the caller has already generated and written the trace as appropriate.
  [[noreturn]] static void FailWithoutStackTrace();

  // Similar to `FailWithoutStackTrace()`, but without `abort()`.  Terminates
  // the process with an error exit code.
  [[noreturn]] static void FailQuietly();

  // After this is called, failures are done as quiet as possible for this log
  // message.
  void SetFailQuietly();

 private:
  struct LogMessageData;  // Opaque type containing message state
  friend class AsLiteralImpl;
  friend class StringifySink;
  template <StructuredStringType str_type>
  friend class AsStructuredStringTypeImpl;
  template <typename T>
  friend class AsStructuredValueImpl;

  // This streambuf writes directly into the structured logging buffer so that
  // arbitrary types can be encoded as string data (using
  // `operator<<(std::ostream &, ...)` without any extra allocation or copying.
  // Space is reserved before the data to store the length field, which is
  // filled in by `~OstreamView`.
  class OstreamView final : public std::streambuf {
   public:
    explicit OstreamView(LogMessageData& message_data);
    ~OstreamView() override;
    OstreamView(const OstreamView&) = delete;
    OstreamView& operator=(const OstreamView&) = delete;
    std::ostream& stream();

   private:
    LogMessageData& data_;
    absl::Span<char> encoded_remaining_copy_;
    absl::Span<char> message_start_;
    absl::Span<char> string_start_;
  };

  void AtLocationImpl(absl::string_view file, int line);
  void NoPrefixImpl();
  void WithVerbosityImpl(int verbose_level);
  void WithTimestampImpl(absl::Time timestamp);
  void WithThreadIDImpl(absl::LogEntry::tid_t tid);
  void WithMetadataFromImpl(const absl::LogEntry& entry);
  void WithPerrorImpl();
  void ToSinkAlsoImpl(absl::LogSink* absl_nonnull sink);
  void ToSinkOnlyImpl(absl::LogSink* absl_nonnull sink);

  template <typename T>
  ABSL_ATTRIBUTE_NOINLINE void Stream(T v);

  enum class StringType {
    kLiteral,
    kNotLiteral,
  };
  template <StringType str_type>
  void CopyToEncodedBuffer(absl::string_view str);
  // Keeps std::string data and size access out of callsites.
  template <StringType str_type>
  void CopyToEncodedBuffer(const std::string& str);
  template <StringType str_type>
  void CopyToEncodedBuffer(char ch, size_t num);
  template <StringType str_type>
  void CopyToEncodedBuffer(std::wstring_view str);

  // Copies `field` to the encoded buffer, then appends `str` after it
  // (truncating `str` if necessary to fit).
  template <StringType str_type>
  void CopyToEncodedBufferWithStructuredProtoField(StructuredProtoField field,
                                                   absl::string_view str);

  // Returns `true` if the message is fatal or enabled debug-fatal.
  bool IsFatal() const;

  // Records some tombstone-type data in anticipation of `Die`.
  void PrepareToDie();
  void Die();

  void SendToLog();

  // Checks `FLAGS_log_backtrace_at` and appends a backtrace if appropriate.
  void LogBacktraceIfNeeded();

  // This should be the first data member so that its initializer captures errno
  // before any other initializers alter it (e.g. with calls to new) and so that
  // no other destructors run afterward an alter it (e.g. with calls to delete).
  absl::base_internal::ErrnoSaver errno_saver_;

  // We keep the data in a separate struct so that each instance of `LogMessage`
  // uses less stack space.
  absl_nonnull std::unique_ptr<LogMessageData> data_;
};

// Explicitly specializes the generic operator<< for `const wchar_t*`
// arguments.
//
// This method is used instead of a non-template `const wchar_t*` overload,
// as the latter was found to take precedence over the array template
// (`operator<<(const wchar_t(&)[SIZE])`) when handling string literals.
// This specialization ensures the array template now correctly processes
// literals.
template <>
LogMessage& LogMessage::operator<< <const wchar_t*>(
    const wchar_t* absl_nullable const& v);

inline LogMessage& LogMessage::operator<<(wchar_t* absl_nullable v) {
  return operator<<(const_cast<const wchar_t*>(v));
}

// Helper class so that `AbslStringify()` can modify the LogMessage.
class StringifySink final {
 public:
  explicit StringifySink(LogMessage& message) : message_(message) {}

  void Append(size_t count, char ch) {
    message_.CopyToEncodedBuffer<LogMessage::StringType::kNotLiteral>(ch,
                                                                      count);
  }

  void Append(absl::string_view v) {
    message_.CopyToEncodedBuffer<LogMessage::StringType::kNotLiteral>(v);
  }

  // For types that implement `AbslStringify` using `absl::Format()`.
  friend void AbslFormatFlush(StringifySink* absl_nonnull sink,
                              absl::string_view v) {
    sink->Append(v);
  }

 private:
  LogMessage& message_;
};

template <typename T>
inline LogMessage& LogMessage::operator<<(const T& v) {
  Stream<const T&>(v);
  return *this;
}

// Note: the following is declared `ABSL_ATTRIBUTE_NOINLINE`
template <typename T>
void LogMessage::Stream(T v) {
  // `T` may be `const U&` (from the generic `operator<<`); strip `&` and
  // `const` (preserving `volatile`) so trait and `NullGuard` lookups see `U`.
  using RawT = std::remove_const_t<std::remove_reference_t<T>>;
  if constexpr (absl::HasAbslStringify<RawT>::value) {
    StringifySink sink(*this);
    // Replace with public API.
    AbslStringify(sink, v);
  } else {
    OstreamView view(*data_);
    view.stream() << log_internal::NullGuard<RawT>().Guard(v);
  }
}

template <int SIZE>
LogMessage& LogMessage::operator<<(const char (&buf)[SIZE]) {
  CopyToEncodedBuffer<StringType::kLiteral>(absl::string_view(buf));
  return *this;
}

template <int SIZE>
LogMessage& LogMessage::operator<<(const wchar_t (&buf)[SIZE]) {
  CopyToEncodedBuffer<StringType::kLiteral>(buf);
  return *this;
}

// Note: the following is declared `ABSL_ATTRIBUTE_NOINLINE`
template <int SIZE>
LogMessage& LogMessage::operator<<(char (&buf)[SIZE]) {
  CopyToEncodedBuffer<StringType::kNotLiteral>(absl::string_view(buf));
  return *this;
}
// We instantiate these specializations in the library's TU to save space in
// other TUs.  Since the template is marked `ABSL_ATTRIBUTE_NOINLINE` we will be
// emitting a function call either way.
// NOLINTBEGIN(runtime/int)
// NOLINTBEGIN(google-runtime-int)
extern template void LogMessage::Stream(char v);
extern template void LogMessage::Stream(signed char v);
extern template void LogMessage::Stream(unsigned char v);
extern template void LogMessage::Stream(short v);
extern template void LogMessage::Stream(unsigned short v);
extern template void LogMessage::Stream(int v);
extern template void LogMessage::Stream(unsigned int v);
extern template void LogMessage::Stream(long v);
extern template void LogMessage::Stream(unsigned long v);
extern template void LogMessage::Stream(long long v);
extern template void LogMessage::Stream(unsigned long long v);
extern template void LogMessage::Stream(void* absl_nullable v);
extern template void LogMessage::Stream(const void* absl_nullable v);
extern template void LogMessage::Stream(float v);
extern template void LogMessage::Stream(double v);
extern template void LogMessage::Stream(bool v);
// NOLINTEND(google-runtime-int)
// NOLINTEND(runtime/int)

extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kLiteral>(absl::string_view str);
extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kNotLiteral>(absl::string_view str);
extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kLiteral>(const std::string& str);
extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kNotLiteral>(const std::string& str);
extern template void
LogMessage::CopyToEncodedBuffer<LogMessage::StringType::kLiteral>(char ch,
                                                                  size_t num);
extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kNotLiteral>(char ch, size_t num);
extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kLiteral>(std::wstring_view str);
extern template void LogMessage::CopyToEncodedBuffer<
    LogMessage::StringType::kNotLiteral>(std::wstring_view str);

// `LogMessageFatal` ensures the process will exit in failure after logging this
// message.
class LogMessageFatal final : public LogMessage {
 public:
  ABSL_ATTRIBUTE_COLD LogMessageFatal(const char* absl_nonnull file, int line);
  ABSL_ATTRIBUTE_COLD LogMessageFatal(const char* absl_nonnull file, int line,
                                      const char* absl_nonnull failure_msg);
  [[noreturn]] ~LogMessageFatal();
};

// `LogMessageDebugFatal` ensures the process will exit in failure after logging
// this message. It matches LogMessageFatal but is not [[noreturn]] as it's used
// for DLOG(FATAL) variants.
class LogMessageDebugFatal final : public LogMessage {
 public:
  ABSL_ATTRIBUTE_COLD LogMessageDebugFatal(const char* absl_nonnull file,
                                           int line);
  ~LogMessageDebugFatal();
};

class LogMessageQuietlyDebugFatal final : public LogMessage {
 public:
  // DLOG(QFATAL) calls this instead of LogMessageQuietlyFatal to make sure the
  // destructor is not [[noreturn]] even if this is always FATAL as this is only
  // invoked when DLOG() is enabled.
  ABSL_ATTRIBUTE_COLD LogMessageQuietlyDebugFatal(const char* absl_nonnull file,
                                                  int line);
  ~LogMessageQuietlyDebugFatal();
};

// Used for LOG(QFATAL) to make sure it's properly understood as [[noreturn]].
class LogMessageQuietlyFatal final : public LogMessage {
 public:
  ABSL_ATTRIBUTE_COLD LogMessageQuietlyFatal(const char* absl_nonnull file,
                                             int line);
  ABSL_ATTRIBUTE_COLD LogMessageQuietlyFatal(
      const char* absl_nonnull file, int line,
      const char* absl_nonnull failure_msg);
  [[noreturn]] ~LogMessageQuietlyFatal();
};

}  // namespace log_internal
ABSL_NAMESPACE_END
}  // namespace absl

extern "C" ABSL_ATTRIBUTE_WEAK void ABSL_INTERNAL_C_SYMBOL(
    AbslInternalOnFatalLogMessage)(const absl::LogEntry&);

#endif  // ABSL_LOG_INTERNAL_LOG_MESSAGE_H_
