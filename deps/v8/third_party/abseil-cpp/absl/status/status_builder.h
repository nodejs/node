// Copyright 2026 The Abseil Authors.
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
// File: status_builder.h
// -----------------------------------------------------------------------------

#ifndef ABSL_STATUS_STATUS_BUILDER_H_
#define ABSL_STATUS_STATUS_BUILDER_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/log_severity.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/internal/ostringstream.h"
#include "absl/strings/internal/stringify_stream.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/source_location.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

class LogSink;
class StatusBuilder;

namespace status_internal {

// StatusBuilder's operator<< overloads use an AbslStringifyStream to allow us
// to use AbslStringify. This wraps (but does not own) an OStringStream, which
// we use for speed. We bundle them together in Stream here, partly for
// convenience in StatusBuilder's implementation, and partly to help make sure
// their lifetimes are managed correctly.
class Stream {
 public:
  explicit Stream(std::string& message)
      : ostringstream_(&message), absl_stringify_stream_(ostringstream_) {}

  template <typename T>
  friend Stream& operator<<(Stream& stream, const T& t) {
    stream.absl_stringify_stream_ << t;
    return stream;
  }

 private:
  absl::strings_internal::OStringStream ostringstream_;
  absl::strings_internal::StringifyStream absl_stringify_stream_;
};

// StatusBuilder::With() adaptors can be classified as either "pure policy" or
// "terminal". Terminal adaptors are a mix of "side effect" or "conversion".
// We differentiate between these types by the functor's return type.
//
// This is currently for analysis only, as part of an ongoing LSC investigation.
template <typename Fn, typename Arg, typename Expected>
inline constexpr bool kResultMatches =
    std::is_same_v<std::decay_t<std::invoke_result_t<Fn, Arg>>, Expected>;

template <typename Adaptor, typename Builder>
using PurePolicy =
    std::enable_if_t<kResultMatches<Adaptor, Builder, StatusBuilder>,
                     std::invoke_result_t<Adaptor, Builder>>;

template <typename Adaptor, typename Builder>
using SideEffect =
    std::enable_if_t<kResultMatches<Adaptor, Builder, absl::Status>,
                     std::invoke_result_t<Adaptor, Builder>>;

template <typename Adaptor, typename Builder>
using Conversion =
    std::enable_if_t<!kResultMatches<Adaptor, Builder, StatusBuilder> &&
                         !kResultMatches<Adaptor, Builder, absl::Status>,
                     std::invoke_result_t<Adaptor, Builder>>;

class StatusBuilderPrivateAccessor;

}  // namespace status_internal

// Internal argument-dependent lookup extension point for StatusBuilder.
// Necessary for allowing StatusBuilder to be open-sourced into Abseil without
// introducing hard dependencies on non-canonical error spaces or protobuf.
//
// What we would have preferred here is to just leave
// StatusBuilder::SetErrorCode() undeclared for OSS users, but defined for
// internal users. However, C++ doesn't provide a great way to do that directly,
// since merely omitting a definition still leaves it declared and thus
// accessible.
//
// We therefore use this ADL extension point as the closest moral equivalent:
// this allows use to define a simple shell for the method in OSS, but to
// prevent its actual usage (via SFINAE) unless this extension point is also
// defined, which is only the case in our internal libraries.
void AbslInternalSetErrorCode(StatusBuilder&, absl::StatusCode);

// Specifies how to join the error message in the original status and any
// additional message that has been streamed into the builder.
enum class MessageJoinStyle {
  kAnnotate,
  kAppend,
  kPrepend,
};

// Creates a status based on an original_status, but enriched with additional
// information.  The builder implicitly converts to Status and StatusOr<T>
// allowing for it to be returned directly.
//
//   StatusBuilder builder(original);
//   builder.SetPayload(proto);
//   builder << "info about error";
//   return builder;
//
// It provides method chaining to simplify typical usage:
//
//   return StatusBuilder(original)
//       .Log(absl::LogSeverity::kWarning) << "oh no!";
//
// In more detail:
// - When the original status is OK, all methods become no-ops and nothing will
//   be logged.
// - Messages streamed into the status builder are collected into a single
//   additional message string.
// - The original Status's message and the additional message are joined
//   together when the result status is built.
// - By default, the messages will be joined as if by `util::Annotate`, which
//   includes a convenience separator between the original message and the
//   additional one.  This behavior can be changed with the `SetAppend()` and
//   `SetPrepend()` methods of the builder.
// - By default, the result status is not logged.  The `Log` and
//   `EmitStackTrace` methods will cause the builder to log the result status
//   when it is built.
// - All side effects (like logging or constructing a stack trace) happen when
//   the builder is converted to a status.
class ABSL_MUST_USE_RESULT StatusBuilder {
 public:
  explicit StatusBuilder();
  ~StatusBuilder();

  // Creates a `StatusBuilder` based on an original status.  If logging is
  // enabled, it will use `location` as the location from which the log message
  // occurs.  A typical user will not specify `location`, allowing it to default
  // to the current location.
  explicit StatusBuilder(
      const absl::Status& original_status,
      absl::SourceLocation location = absl::SourceLocation::current());
  explicit StatusBuilder(
      absl::Status&& original_status,
      absl::SourceLocation location = absl::SourceLocation::current());

  // Creates a `StatusBuilder` from a status code.  If logging is enabled, it
  // will use `location` as the location from which the log message occurs.  A
  // typical user will not specify `location`, allowing it to default to the
  // current location.
  explicit StatusBuilder(
      absl::StatusCode code,
      absl::SourceLocation location = absl::SourceLocation::current());

  StatusBuilder(const StatusBuilder& sb);
  StatusBuilder& operator=(const StatusBuilder& sb);
  StatusBuilder(StatusBuilder&&) = default;
  StatusBuilder& operator=(StatusBuilder&&) = default;

  // Mutates the builder so that the final additional message is prepended to
  // the original error message in the status.  A convenience separator is not
  // placed between the messages.
  //
  // NOTE: Multiple calls to `SetPrepend` and `SetAppend` just adjust the
  // behavior of the final join of the original status with the extra message.
  //
  // Returns `*this` to allow method chaining.
  StatusBuilder& SetPrepend() &;
  ABSL_MUST_USE_RESULT StatusBuilder&& SetPrepend() &&;

  // Mutates the builder so that the final additional message is appended to the
  // original error message in the status.  A convenience separator is not
  // placed between the messages.
  //
  // NOTE: Multiple calls to `SetPrepend` and `SetAppend` just adjust the
  // behavior of the final join of the original status with the extra message.
  //
  // Returns `*this` to allow method chaining.
  StatusBuilder& SetAppend() &;
  ABSL_MUST_USE_RESULT StatusBuilder&& SetAppend() &&;

  // Mutates the builder to disable any logging that was set using any of the
  // logging functions below.  Returns `*this` to allow method chaining.
  StatusBuilder& SetNoLogging() &;
  ABSL_MUST_USE_RESULT StatusBuilder&& SetNoLogging() &&;

  // Mutates the builder so that the result status will be logged (without a
  // stack trace) when this builder is converted to a Status.  This overrides
  // the logging settings from earlier calls to any of the logging mutator
  // functions.  Returns `*this` to allow method chaining.
  StatusBuilder& Log(absl::LogSeverity level) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& Log(absl::LogSeverity level) &&;

  StatusBuilder& LogError() & { return Log(absl::LogSeverity::kError); }
  ABSL_MUST_USE_RESULT StatusBuilder&& LogError() && {
    return std::move(LogError());
  }
  StatusBuilder& LogWarning() & { return Log(absl::LogSeverity::kWarning); }
  ABSL_MUST_USE_RESULT StatusBuilder&& LogWarning() && {
    return std::move(LogWarning());
  }
  StatusBuilder& LogInfo() & { return Log(absl::LogSeverity::kInfo); }
  ABSL_MUST_USE_RESULT StatusBuilder&& LogInfo() && {
    return std::move(LogInfo());
  }

  // Mutates the builder so that the result status will be logged every N
  // invocations (without a stack trace) when this builder is converted to a
  // Status.  This overrides the logging settings from earlier calls to any of
  // the logging mutator functions.  Returns `*this` to allow method chaining.
  StatusBuilder& LogEveryN(absl::LogSeverity level, int n) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& LogEveryN(absl::LogSeverity level,
                                                 int n) &&;

  // Mutates the builder so that the result status will be logged once per
  // period (without a stack trace) when this builder is converted to a Status.
  // This overrides the logging settings from earlier calls to any of the
  // logging mutator functions.  Returns `*this` to allow method chaining.
  // If period is absl::ZeroDuration() or less, then this is equivalent to
  // calling the Log() method.
  StatusBuilder& LogEvery(absl::LogSeverity level, absl::Duration period) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& LogEvery(absl::LogSeverity level,
                                                absl::Duration period) &&;

  // Mutates the builder so that the result status will be VLOGged (without a
  // stack trace) when this builder is converted to a Status.  `verbose_level`
  // indicates the verbosity level that would be passed to VLOG().  This
  // overrides the logging settings from earlier calls to any of the logging
  // mutator functions.  Returns `*this` to allow method chaining.
  StatusBuilder& VLog(int verbose_level) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& VLog(int verbose_level) &&;

  // Mutates the builder so that a stack trace will be logged if the status is
  // logged. One of the logging setters above should be called as well. If
  // logging is not yet enabled this behaves as if LogInfo().EmitStackTrace()
  // was called. Returns `*this` to allow method chaining.
  StatusBuilder& EmitStackTrace() &;
  ABSL_MUST_USE_RESULT StatusBuilder&& EmitStackTrace() &&;

  // Mutates the builder so that the result status will also be logged to the
  // provided `sink` when this builder is converted to a status. Overwrites any
  // sink set prior. The provided `sink` must point to a valid object by the
  // time this builder is converted to a status. Has no effect if this builder
  // is not configured to log by calling any of the LogXXX methods. Returns
  // `*this` to allow method chaining.
  StatusBuilder& AlsoOutputToSink(absl::LogSink* sink) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& AlsoOutputToSink(absl::LogSink* sink) &&;

  // Mutates the builder so that the result status will only be logged to the
  // provided `sink` when this builder is converted to a status. Overwrites any
  // sink set prior. The provided `sink` must point to a valid object by the
  // time this builder is converted to a status. Has no effect if this builder
  // is not configured to log by calling any of the LogXXX methods. Returns
  // `*this` to allow method chaining.
  StatusBuilder& OnlyOutputToSink(absl::LogSink* sink) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& OnlyOutputToSink(absl::LogSink* sink) &&;

  // Appends to the extra message that will be added to the original status.  By
  // default, the extra message is added to the original message as if by
  // `util::Annotate`, which includes a convenience separator between the
  // original message and the enriched one.
  template <typename T>
  StatusBuilder& operator<<(const T& value) &;

  template <typename T>
  ABSL_MUST_USE_RESULT StatusBuilder&& operator<<(const T& value) &&;

  // Adds a payload for the status that will be returned by this StatusBuilder.
  // Note that this is equivalent to `Status::SetPayload`, to attach protos to
  // the MessageSet payload, use `util::AttachPayload`. Returns '*this' to allow
  // method chaining.
  StatusBuilder& SetPayload(absl::string_view type_url, absl::Cord payload) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& SetPayload(absl::string_view type_url,
                                                  absl::Cord payload) && {
    return std::move(SetPayload(type_url, std::move(payload)));
  }

  std::optional<absl::Cord> GetPayload(absl::string_view type_url) const;

  // INTERNAL API. NOT FOR PUBLIC USE.
  template <typename MessageSetExtension, typename ExtensionIdentifier>
  auto AttachPayload(const MessageSetExtension& obj,
                     const ExtensionIdentifier& id) &  //
      -> std::enable_if_t<
          std::is_void_v<decltype(AbslInternalAttachPayload(*this, obj, id))>,
          StatusBuilder&> {
    AbslInternalAttachPayload(*this, obj, id);
    return *this;
  }

  // As above, but &&-qualified.
  //
  // Uses decltype() to propagate template constraints (SFINAE).
  template <typename MessageSetExtension, typename ExtensionIdentifier>
  ABSL_MUST_USE_RESULT auto AttachPayload(const MessageSetExtension& obj,
                                          const ExtensionIdentifier& id) &&  //
      -> decltype(std::move(AttachPayload(obj, id))) {
    return std::move(AttachPayload(obj, id));
  }

  // Like `AttachPayload() &`, but with the default extension ID.
  //
  // INTERNAL API. NOT FOR PUBLIC USE.
  template <typename MessageSetExtension>
  auto AttachPayload(const MessageSetExtension& obj) &  //
      -> std::enable_if_t<
          std::is_void_v<decltype(AbslInternalAttachPayload(*this, obj))>,
          StatusBuilder&> {
    AbslInternalAttachPayload(*this, obj);
    return *this;
  }

  // As above, but &&-qualified.
  //
  // Uses decltype() to propagate template constraints (SFINAE).
  template <typename MessageSetExtension>
  ABSL_MUST_USE_RESULT auto AttachPayload(const MessageSetExtension& obj) &&  //
      -> decltype(std::move(AttachPayload(obj))) {
    return std::move(AttachPayload(obj));
  }

  // HasPayload()
  //
  // Indicates whether the Status object that will be returned by the
  // StatusBuilder contains any payloads with a type extending proto2's
  // `MessageSet`, returning `true` if so. Having a payload does not guarantee
  // the presence of a payload with a specific type. Note that returning `false`
  // does not necessarily indicate the absence of a payload, but only the
  // absence on one which extends `MessageSet`.
  bool HasPayload() const;

  // INTERNAL API. NOT FOR PUBLIC USE.
  template <typename Enum>
  auto SetErrorCode(Enum code) &  //
      -> std::enable_if_t<
          std::is_void_v<decltype(AbslInternalSetErrorCode(*this, code))>,
          StatusBuilder&> {
    AbslInternalSetErrorCode(*this, code);
    return *this;
  }

  // As above, but &&-qualified.
  //
  // Uses decltype() to propagate template constraints (SFINAE).
  template <typename Enum>
  ABSL_MUST_USE_RESULT auto SetErrorCode(Enum code) &&  //
      -> decltype(std::move(SetErrorCode(code))) {
    return std::move(SetErrorCode(code));
  }

  // Sets the status code for the status that will be returned by this
  // StatusBuilder. Returns `*this` to allow method chaining.
  StatusBuilder& SetCode(absl::StatusCode code) &;
  ABSL_MUST_USE_RESULT StatusBuilder&& SetCode(absl::StatusCode code) && {
    return std::move(SetCode(code));
  }

  ///////////////////////////////// Adaptors /////////////////////////////////
  //
  // A StatusBuilder `adaptor` is a functor which can be included in a builder
  // method chain. There are two common variants:
  //
  // 1. `Pure policy` adaptors modify the StatusBuilder and return the modified
  //    object, which can then be chained with further adaptors or mutations.
  //
  // 2. `Terminal` adaptors consume the builder's Status and return some
  //    other type of object. Alternatively, the consumed Status may be used
  //    for side effects, e.g. by passing it to a side channel. A terminal
  //    adaptor cannot be chained.
  //
  // Careful: The conversion of StatusBuilder to Status has side effects!
  // Adaptors must ensure that this conversion happens at most once in the
  // builder chain. The easiest way to do this is to determine the adaptor type
  // and follow the corresponding guidelines:
  //
  // Pure policy adaptors should:
  // (a) Take a StatusBuilder as input parameter.
  // (b) NEVER convert the StatusBuilder to Status:
  //     - Never assign the builder to a Status variable.
  //     - Never pass the builder to a function whose parameter type is Status,
  //       including by reference (e.g. const Status&).
  //     - Never pass the builder to any function which might convert the
  //       builder to Status (i.e. this restriction is viral).
  // (c) Return a StatusBuilder (usually the input parameter).
  //
  // Terminal adaptors should:
  // (a) Take a Status as input parameter (not a StatusBuilder!).
  // (b) Return a type matching the enclosing function. (This can be `void`.)
  //
  // Adaptors do not necessarily fit into one of these categories. However, any
  // which satisfies the conversion rule can always be decomposed into a pure
  // adaptor chained into a terminal adaptor. (This is recommended.)
  //
  // Examples
  //
  // Pure adaptors allow teams to configure team-specific error handling
  // policies.  For example:
  //
  //   StatusBuilder TeamPolicy(StatusBuilder builder) {
  //     builder.SetPayload(...);
  //     return std::move(builder).Log(absl::LogSeverity::kWarning);
  //   }
  //
  //   ABSL_RETURN_IF_ERROR(foo()).With(TeamPolicy);
  //
  // Because pure policy adaptors return the modified StatusBuilder, they
  // can be chained with further adaptors, e.g.:
  //
  //   ABSL_RETURN_IF_ERROR(foo()).With(TeamPolicy).With(OtherTeamPolicy);
  //
  // Terminal adaptors are often used for type conversion. This allows
  // ABSL_RETURN_IF_ERROR to be used in functions which do not return Status.
  // For example, a function might want to return some default value on error:
  //
  //   int GetSysCounter() {
  //     int value;
  //     ABSL_RETURN_IF_ERROR(ReadCounterFile(filename, &value))
  //         .LogInfo()
  //         .With([](const absl::Status& unused) { return 0; });
  //     return value;
  //   }
  //
  // For the simple case of returning a constant (e.g. zero, false, nullptr) on
  // error, consider `status_macros::Return` or `status_macros::ReturnVoid`:
  //
  //   #include "util/task/contrib/status_macros/return.h"
  //
  //   bool DoMyThing() {
  //     ABSL_RETURN_IF_ERROR(foo()).LogWarning().With(status_macros::Return(false));
  //     ...
  //   }
  //
  // A terminal adaptor may instead (or additionally) be used to create side
  // effects that are not supported natively by `StatusBuilder`, such as
  // returning the Status through a side channel. For example,
  // `util::TaskReturn` returns the Status through the `util::Task` that it was
  // initialized with. This adaptor then returns `void`, to match the typical
  // return type of functions that maintain state through `util::Task`:
  //
  //   class TaskReturn {
  //    public:
  //     explicit TaskReturn(Task* t) : task_(t) {}
  //     void operator()(const Status& status) const { task_->Return(status); }
  //     // ...
  //   };
  //
  //   void Read(absl::string_view name, util::Task* task) {
  //     int64 id;
  //     ABSL_RETURN_IF_ERROR(GetIdForName(name, &id)).With(TaskReturn(task));
  //     ABSL_RETURN_IF_ERROR(ReadForId(id)).With(TaskReturn(task));
  //     task->Return();
  //   }

  // Calls `adaptor` on this status builder to apply policies, type conversions,
  // and/or side effects on the StatusBuilder. Returns the value returned by
  // `adaptor`, which may be any type including `void`. See comments above.
  //
  // Style guide exception for Ref qualified methods and rvalue refs
  // (cl/128258530).  This allows us to avoid a copy in the common case.
  //
  // Note: All With() overrides are equivalent, and return Adaptor(this). They
  // are part of an ongoing LSC investigation.
  template <typename Adaptor>
  auto With(Adaptor&& adaptor) & -> status_internal::PurePolicy<
      Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }
  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&&
          adaptor) && -> status_internal::PurePolicy<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

  template <typename Adaptor>
  auto With(Adaptor&& adaptor) & -> status_internal::SideEffect<
      Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }
  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&&
          adaptor) && -> status_internal::SideEffect<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

  template <typename Adaptor>
  auto With(Adaptor&& adaptor) & -> status_internal::Conversion<
      Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }
  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&&
          adaptor) && -> status_internal::Conversion<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

  // Returns true if the Status created by this builder will be ok().
  ABSL_MUST_USE_RESULT bool ok() const;

  // Returns the (canonical) error code for the Status created by this builder.
  absl::StatusCode code() const;

  // Implicit conversion to Status.
  //
  // Careful: this operator has side effects, so it should be called at
  // most once. In particular, do NOT use this conversion operator to inspect
  // the status from an adapter object passed into With().
  //
  // Style guide exception for using Ref qualified methods and for implicit
  // conversions (cl/124566728).  This override allows us to implement
  // ABSL_RETURN_IF_ERROR with 2 move operations in the common case.
  operator absl::Status() const&;  // NOLINT: Builder converts implicitly.
  operator absl::Status() &&;      // NOLINT: Builder converts implicitly.

  // Returns the source location used to create this builder. This differs from
  // `GetPreviousSourceLocations()` as this location is the single location for
  // the creation of this builder.
  ABSL_MUST_USE_RESULT absl::SourceLocation source_location() const;

  // Returns the source locations previously recorded in the status. This will
  // not include the source location of the `StatusBuilder` itself (which is
  // available via `source_location()`). When the builder is converted to a
  // Status, the builder's location will be appended to this list of previous
  // locations. For `StatusBuilder`s created without an original status (e.g.,
  // from a status code), this will be empty.
  decltype(auto) GetPreviousSourceLocations() const {
    if (rep_ == nullptr) {
      return absl::OkStatus().GetSourceLocations();
    }
    return rep_->status.GetSourceLocations();
  }

  // Returns a string based on the `mode`. This produces the same string as
  // would converting to a Status and calling operator<<, but it does so without
  // side effects (e.g., logging). Therefore, it is safe to use from within an
  // adapter object passed into With().
  std::string ToString() const;

 private:
  friend class status_internal::StatusBuilderPrivateAccessor;

  // Returns true if the compiler can determine that the instance is empty. That
  // is, `rep_ == nullptr`.
  // A `false` return does not necessarily indicate that it has a rep. It just
  // can't prove it doesn't.
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool IsKnownToBeEmpty() const {
#if ABSL_HAVE_BUILTIN(__builtin_constant_p)
    // __builtin_constant_p does not like it when it has non trivial expressions
    // in it, and `rep_==nullptr` is a user-defined operator.
    // Do it out of the builtin and pass the bool instead.
    bool is_empty = rep_ == nullptr;
    return __builtin_constant_p(is_empty) && is_empty;
#else
    return false;
#endif
  }

  // Tells the compiler that it can assume that `rep_` is null.
  // This is verified in non-opt mode.
  void AssumeEmpty() const {
    if (rep_ != nullptr) ABSL_UNREACHABLE();
  }

  static std::string CurrentStackTrace();

  struct Rep;
  // Destroy the `Rep` object. It is just calling the destructor of the
  // `unique_ptr`, but out of line.
  static void Destroy(std::unique_ptr<Rep>);

  // Creates a Status from this builder and logs it if the builder has been
  // configured to log itself.
  // NOTE: This function is `static` to prevent escaping the `this` pointer. We
  //       transfer the `Rep` to delegate the destruction.
  static absl::Status CreateStatusAndConditionallyLog(absl::SourceLocation loc,
                                                      std::unique_ptr<Rep> rep);

  // Infrequently set builder options, instantiated lazily. This reduces
  // average construction/destruction time (e.g. the `stream` is fairly
  // expensive). Stacks can also be blown if StatusBuilder grows too large.
  // This is primarily an issue for debug builds, which do not necessarily
  // re-use stack space within a function across the sub-scopes used by
  // status macros.
  struct Rep {
    explicit Rep(const absl::Status& s);
    explicit Rep(absl::Status&& s);
    Rep(const Rep& r);
    ~Rep();
    void InitStream();

    // The status that the result will be based on.  Can be modified by
    // SetPayload().
    absl::Status status;

    enum class LoggingMode {
      kDisabled,
      kLog,
      kVLog,
      kLogEveryN,
      kLogEveryPeriod
    };
    LoggingMode logging_mode = LoggingMode::kDisabled;

    // The severity level at which the Status should be logged. Note that
    // `logging_mode == LoggingMode::kVLog` always logs at severity INFO.
    absl::LogSeverity log_severity;

    // The level at which the Status should be VLOGged.
    // Only used when `logging_mode == LoggingMode::kVLog`.
    int verbose_level;

    // Only log every N invocations.
    // Only used when `logging_mode == LoggingMode::kLogEveryN`.
    int n;

    // Only log once per period.
    // Only used when `logging_mode == LoggingMode::kLogEveryPeriod`.
    absl::Duration period;

    // Gathers additional messages added with `<<` for use in the final status.
    std::string stream_message;
    std::optional<status_internal::Stream> stream;

    // If not nullptr, specifies the log sink where log output should be
    // sent to.  Only used when `logging_mode != LoggingMode::kDisabled`.
    absl::LogSink* sink = nullptr;

    // Specifies how to join the message in `status` and `stream`.
    MessageJoinStyle message_join_style = MessageJoinStyle::kAnnotate;

    // Whether to log stack trace.  Only used when `logging_mode !=
    // LoggingMode::kDisabled`.
    bool should_log_stack_trace = false;

    // When this is true, will send both to `sink` and will log. When this is
    // false, will only send to `sink`.
    bool also_send_to_log = true;
  };

  static Rep* InitRep(const absl::Status& s) {
    if (s.ok()) {
      return nullptr;
    } else {
      return new Rep(s);
    }
  }

  static Rep* InitRep(absl::Status&& s) {
    if (s.ok()) {
      return nullptr;
    } else {
      Rep* rep = InitRepImpl(std::move(s));
      ABSL_ASSUME(rep != nullptr);
      return rep;
    }
  }

  // Out of line function that constructs the Rep.
  // Note that `s` is by value because of the TRIVIAL_ABI.
  static Rep* InitRepImpl(absl::Status s);

  // The location to record if this status is logged.
  absl::SourceLocation loc_;

  // nullptr if the result status will be OK.  Extra fields moved to the heap to
  // minimize stack space.
  std::unique_ptr<Rep> rep_;

  // For testing Rep's implememntation.
  friend class StatusBuilderTest;
};

// Implicitly converts `builder` to `Status` and write it to `os`.
std::ostream& operator<<(std::ostream& os, const StatusBuilder& builder);
std::ostream& operator<<(std::ostream& os, StatusBuilder&& builder);

// StatusBuilder policy to append an extra message to the original status.
//
// This is most useful with adaptors such as util::TaskReturn that otherwise
// would prevent use of operator<<.  For example:
//
//   ABSL_RETURN_IF_ERROR(foo(val))
//       .With(util::ExtraMessage("when calling foo()"))
//       .With(util::TaskReturn(task));
//
// or
//
//   ABSL_RETURN_IF_ERROR(foo(val))
//       .With(util::ExtraMessage() << "val: " << val)
//       .With(util::TaskReturn(task));
//
// Note in the above example, the ABSL_RETURN_IF_ERROR macro ensures the
// ExtraMessage expression is evaluated only in the error case, so efficiency of
// constructing the message is not a concern in the success case.
class ExtraMessage {
 public:
  ExtraMessage() : ExtraMessage(std::string()) {}
  explicit ExtraMessage(std::string msg)
      : msg_(std::move(msg)), stream_(msg_) {}

  ExtraMessage(
      ExtraMessage&& other) noexcept  // strings::OStringStream is stateless
                                      // so we can simply move over the message.
      : ExtraMessage(std::move(other.msg_)) {}

  // Appends to the extra message that will be added to the original status.  By
  // default, the extra message is added to the original message as if by
  // `util::Annotate`, which includes a convenience separator between the
  // original message and the enriched one.
  template <typename T>
  ExtraMessage& operator<<(const T& value) & {
    stream_ << value;
    return *this;
  }

  // As above, preserving the rvalue-ness of the ExtraMessage object.
  template <typename T>
  ExtraMessage&& operator<<(const T& value) && {
    *this << value;
    return std::move(*this);
  }

  // Appends to the extra message that will be added to the original status.  By
  // default, the extra message is added to the original message as if by
  // `util::Annotate`, which includes a convenience separator between the
  // original message and the enriched one.
  StatusBuilder operator()(StatusBuilder builder) const {
    builder << msg_;
    return builder;
  }

 private:
  std::string msg_;
  status_internal::Stream stream_;
};

// Implementation details follow; clients should ignore.

inline StatusBuilder::StatusBuilder(absl::StatusCode code,
                                    absl::SourceLocation location)
    : loc_(location), rep_(InitRep(absl::Status(code, ""))) {}

inline StatusBuilder::StatusBuilder(const StatusBuilder& sb) : loc_(sb.loc_) {
  if (sb.rep_ != nullptr) {
    rep_ = std::make_unique<Rep>(*sb.rep_);
  }
}

inline StatusBuilder::StatusBuilder(absl::Status&& original_status,
                                    absl::SourceLocation location)
    : loc_(location), rep_(InitRep(std::move(original_status))) {}

inline StatusBuilder::~StatusBuilder() {
  if (IsKnownToBeEmpty()) {
    // Nothing to do.
    return;
  }
  // We will run the destructor logic, so move it out of line.
  // The destructor of the unique_ptr runs ~Rep() and then ::operator delete.
  // We don't want that bloat on the caller.
  Destroy(std::move(rep_));
  // Tell the compiler that `rep_` was not filled again even if `this` escaped.
  AssumeEmpty();
}

inline StatusBuilder& StatusBuilder::operator=(const StatusBuilder& sb) {
  loc_ = sb.loc_;
  if (sb.rep_ != nullptr) {
    rep_ = std::make_unique<Rep>(*sb.rep_);
  } else {
    rep_ = nullptr;
  }
  return *this;
}

inline StatusBuilder& StatusBuilder::SetPrepend() & {
  if (rep_ == nullptr) return *this;
  rep_->message_join_style = MessageJoinStyle::kPrepend;
  return *this;
}
inline StatusBuilder&& StatusBuilder::SetPrepend() && {
  return std::move(SetPrepend());
}

inline StatusBuilder& StatusBuilder::SetAppend() & {
  if (rep_ == nullptr) return *this;
  rep_->message_join_style = MessageJoinStyle::kAppend;
  return *this;
}
inline StatusBuilder&& StatusBuilder::SetAppend() && {
  return std::move(SetAppend());
}

inline StatusBuilder& StatusBuilder::SetNoLogging() & {
  if (rep_ != nullptr) {
    rep_->logging_mode = Rep::LoggingMode::kDisabled;
    rep_->should_log_stack_trace = false;
  }
  return *this;
}
inline StatusBuilder&& StatusBuilder::SetNoLogging() && {
  return std::move(SetNoLogging());
}

inline StatusBuilder& StatusBuilder::Log(absl::LogSeverity level) & {
  if (rep_ == nullptr) return *this;
  rep_->logging_mode = Rep::LoggingMode::kLog;
  rep_->log_severity = level;
  return *this;
}
inline StatusBuilder&& StatusBuilder::Log(absl::LogSeverity level) && {
  return std::move(Log(level));
}

inline StatusBuilder& StatusBuilder::LogEveryN(absl::LogSeverity level,
                                               int n) & {
  if (rep_ == nullptr) return *this;
  if (n < 1) return Log(level);
  rep_->logging_mode = Rep::LoggingMode::kLogEveryN;
  rep_->log_severity = level;
  rep_->n = n;
  return *this;
}
inline StatusBuilder&& StatusBuilder::LogEveryN(absl::LogSeverity level,
                                                int n) && {
  return std::move(LogEveryN(level, n));
}

inline StatusBuilder& StatusBuilder::LogEvery(absl::LogSeverity level,
                                              absl::Duration period) & {
  if (rep_ == nullptr) return *this;
  if (period <= absl::ZeroDuration()) return Log(level);
  rep_->logging_mode = Rep::LoggingMode::kLogEveryPeriod;
  rep_->log_severity = level;
  rep_->period = period;
  return *this;
}
inline StatusBuilder&& StatusBuilder::LogEvery(absl::LogSeverity level,
                                               absl::Duration period) && {
  return std::move(LogEvery(level, period));
}

inline StatusBuilder& StatusBuilder::VLog(int verbose_level) & {
  if (rep_ == nullptr) return *this;
  rep_->logging_mode = Rep::LoggingMode::kVLog;
  rep_->verbose_level = verbose_level;
  return *this;
}
inline StatusBuilder&& StatusBuilder::VLog(int verbose_level) && {
  return std::move(VLog(verbose_level));
}

inline StatusBuilder& StatusBuilder::EmitStackTrace() & {
  if (rep_ == nullptr) return *this;
  if (rep_->logging_mode == Rep::LoggingMode::kDisabled) {
    // Default to INFO logging, otherwise nothing would be emitted.
    rep_->logging_mode = Rep::LoggingMode::kLog;
    rep_->log_severity = absl::LogSeverity::kInfo;
  }
  rep_->should_log_stack_trace = true;
  return *this;
}
inline StatusBuilder&& StatusBuilder::EmitStackTrace() && {
  return std::move(EmitStackTrace());
}

inline StatusBuilder& StatusBuilder::AlsoOutputToSink(absl::LogSink* sink) & {
  if (rep_ == nullptr) return *this;
  rep_->sink = sink;
  rep_->also_send_to_log = true;
  return *this;
}
inline StatusBuilder&& StatusBuilder::AlsoOutputToSink(absl::LogSink* sink) && {
  return std::move(AlsoOutputToSink(sink));
}
inline StatusBuilder& StatusBuilder::OnlyOutputToSink(absl::LogSink* sink) & {
  if (rep_ == nullptr) return *this;
  rep_->sink = sink;
  rep_->also_send_to_log = false;
  return *this;
}
inline StatusBuilder&& StatusBuilder::OnlyOutputToSink(absl::LogSink* sink) && {
  return std::move(OnlyOutputToSink(sink));
}

template <typename T>
StatusBuilder& StatusBuilder::operator<<(const T& value) & {
  if (rep_ == nullptr) return *this;
  if (!rep_->stream.has_value()) {
    rep_->InitStream();
  }
  *rep_->stream << value;
  return *this;
}

template <typename T>
StatusBuilder&& StatusBuilder::operator<<(const T& value) && {
  return std::move(operator<<(value));
}

inline StatusBuilder& StatusBuilder::SetPayload(absl::string_view type_url,
                                                absl::Cord payload) & {
  if (rep_ != nullptr) {
    rep_->status.SetPayload(type_url, std::move(payload));
  }
  return *this;
}

inline std::optional<absl::Cord> StatusBuilder::GetPayload(
    absl::string_view type_url) const {
  return rep_ == nullptr ? std::nullopt : rep_->status.GetPayload(type_url);
}

inline bool StatusBuilder::ok() const {
  return rep_ == nullptr ? true : rep_->status.ok();
}

inline absl::StatusCode StatusBuilder::code() const {
  return rep_ == nullptr ? absl::StatusCode::kOk : rep_->status.code();
}

inline StatusBuilder::operator absl::Status() && {
  // Tell the compiler that the `ok()` path will return an `Ok` status, but do
  // it only if the compiler can determine it at compile time.
  // When it can't, we delegate this check into the out of line function.
  if (IsKnownToBeEmpty()) {
    return absl::OkStatus();
  }
  absl::Status result = CreateStatusAndConditionallyLog(loc_, std::move(rep_));
  // Tell the compiler that `rep_` was not filled again even if `this` escaped.
  AssumeEmpty();
  return result;
}

inline absl::SourceLocation StatusBuilder::source_location() const {
  return loc_;
}

// HasPayload()
//
// Indicates whether the Status object that will be returned by the
// StatusBuilder contains any payloads with a type extending proto2's
// `MessageSet`, returning `true` if so. Having a payload does not guarantee the
// presence of a payload with a specific type. Note that returning `false` does
// not necessarily indicate the absence of a payload, but only the absence on
// one which extends `MessageSet`.
inline bool HasPayload(const StatusBuilder& builder) {
  return builder.HasPayload();
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STATUS_STATUS_BUILDER_H_
