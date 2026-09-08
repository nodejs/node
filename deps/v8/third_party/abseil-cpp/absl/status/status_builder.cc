// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/status/status_builder.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/source_location.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

void StatusBuilder::Destroy(std::unique_ptr<Rep>) {
  // nothing to do. The unique_ptr will do the cleanup.
}

// These constructors are not-inlined and defined in the .cc file to reduce
// binary size. See cl/354351433 for a quantification.
StatusBuilder::StatusBuilder() {}

StatusBuilder::StatusBuilder(const absl::Status& original_status,
                             absl::SourceLocation location)
    : loc_(location), rep_(InitRep(original_status)) {}

StatusBuilder::operator absl::Status() const& {
  if (rep_ == nullptr) return absl::Status();
  return CreateStatusAndConditionallyLog(loc_, std::make_unique<Rep>(*rep_));
}

StatusBuilder::Rep::Rep(const absl::Status& s) : status(s) {}
StatusBuilder::Rep::Rep(absl::Status&& s) : status(std::move(s)) {}
StatusBuilder::Rep::~Rep() {}

StatusBuilder::Rep* StatusBuilder::InitRepImpl(absl::Status s) {
  if (s.ok()) {
    return nullptr;
  } else {
    return new Rep(std::move(s));
  }
}

StatusBuilder::Rep::Rep(const Rep& r)
    : status(r.status),
      logging_mode(r.logging_mode),
      log_severity(r.log_severity),
      verbose_level(r.verbose_level),
      n(r.n),
      period(r.period),
      stream_message(r.stream_message),
      sink(r.sink),
      message_join_style(r.message_join_style),
      should_log_stack_trace(r.should_log_stack_trace),
      also_send_to_log(r.also_send_to_log) {
  if (r.stream.has_value()) {
    InitStream();
  }
}

void StatusBuilder::Rep::InitStream() { stream.emplace(stream_message); }

bool StatusBuilder::HasPayload() const {
  static constexpr absl::string_view kMessageSetUrl =
      "type.googleapis.com/util.MessageSetPayload";
  return rep_ != nullptr && rep_->status.GetPayload(kMessageSetUrl).has_value();
}

ABSL_ATTRIBUTE_WEAK StatusBuilder& StatusBuilder::SetCode(
    absl::StatusCode code) & {
  if (rep_ == nullptr) {
    rep_ = std::make_unique<StatusBuilder::Rep>(
        absl::Status(code, absl::string_view(), absl::SourceLocation()));
  } else {
    absl::Status status(code, absl::string_view(), absl::SourceLocation());
    rep_->status.ForEachPayload(
        [&status](absl::string_view type_url, const absl::Cord& payload) {
          status.SetPayload(type_url, payload);
        });
    rep_->status = std::move(status);
  }
  return *this;
}

ABSL_ATTRIBUTE_WEAK void AbslInternalSetErrorCode(StatusBuilder& builder,
                                                  absl::StatusCode code) {
  builder.SetCode(code);
}

class status_internal::StatusPrivateAccessorForStatusBuilder {
 public:
  static absl::Status SetMessage(const absl::Status& status,
                                 absl::string_view message) {
    ABSL_ASSERT(!status.ok());

    if (message.empty()) {
      return absl::Status(status.code(), message, absl::SourceLocation());
    }

    using StatusRep =
        std::remove_cv_t<std::remove_pointer_t<decltype(Status::RepToPointer(
            std::declval<uintptr_t>()))>>;
    StatusRep* rep;
    if (Status::IsInlined(status.rep_)) {
      rep = new StatusRep(Status::InlinedRepToCode(status.rep_), message,
                          nullptr);
    } else {
      rep = Status::RepToPointer(status.rep_)->Clone(message, true, true);
    }
    return absl::Status(Status::PointerToRep(rep));
  }

  static absl::Status JoinMessageToStatus(absl::Status s, absl::string_view msg,
                                          MessageJoinStyle style) {
    if (s.ok() || msg.empty()) return s;
    const absl::string_view original_message = s.message();
    switch (style) {
      case MessageJoinStyle::kAnnotate: {
        std::string annotated;
        if (!original_message.empty()) {
          absl::StrAppend(&annotated, original_message, "; ", msg);
          msg = annotated;
        }
        return SetMessage(s, msg);
      }
      case MessageJoinStyle::kPrepend:
        return SetMessage(s, absl::StrCat(msg, original_message));
      case MessageJoinStyle::kAppend:
        return SetMessage(s, absl::StrCat(original_message, msg));
      default:
        return absl::InternalError("Unknown MessageJoinStyle");
    }
  }
};

ABSL_ATTRIBUTE_WEAK std::string StatusBuilder::CurrentStackTrace() {
  return std::string();
}

ABSL_ATTRIBUTE_WEAK absl::Status StatusBuilder::CreateStatusAndConditionallyLog(
    absl::SourceLocation loc, std::unique_ptr<Rep> rep) {
  if (rep == nullptr) return absl::OkStatus();
  absl::Status result = status_internal::StatusPrivateAccessorForStatusBuilder::
      JoinMessageToStatus(std::move(rep->status), rep->stream_message,
                          rep->message_join_style);
  // Passing in the `loc` last to ensure the sequence of the source locations.
  result.AddSourceLocation(loc);
  return result;
}

ABSL_ATTRIBUTE_WEAK std::string StatusBuilder::ToString() const {
  if (rep_ == nullptr) {
    return absl::OkStatus().ToString();
  }

  return status_internal::StatusPrivateAccessorForStatusBuilder::
      JoinMessageToStatus(rep_->status, rep_->stream_message,
                          rep_->message_join_style)
          .WithSourceLocation(loc_)
          .ToString();
}

ABSL_ATTRIBUTE_WEAK std::ostream& operator<<(std::ostream& os,
                                             const StatusBuilder& builder) {
  return os << static_cast<absl::Status>(builder);
}

ABSL_ATTRIBUTE_WEAK std::ostream& operator<<(std::ostream& os,
                                             StatusBuilder&& builder) {
  return os << static_cast<absl::Status>(std::move(builder));
}

ABSL_NAMESPACE_END
}  // namespace absl
