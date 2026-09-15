// Copyright 2019 The Abseil Authors.
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

#include "absl/status/status.h"

#include <errno.h>

#include <array>
#include <cstddef>
#include <sstream>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"

namespace {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::UnorderedElementsAreArray;

TEST(StatusCode, InsertionOperator) {
  const absl::StatusCode code = absl::StatusCode::kUnknown;
  std::ostringstream oss;
  oss << code;
  EXPECT_EQ(oss.str(), absl::StatusCodeToString(code));
  EXPECT_EQ(oss.str(), absl::StatusCodeToStringView(code));
}

// This structure holds the details for testing a single error code,
// its creator, and its classifier.
struct ErrorTest {
  absl::StatusCode code;
  using Creator = absl::Status (*)(absl::string_view, absl::SourceLocation);
  using Classifier = bool (*)(const absl::Status&);
  Creator creator;
  Classifier classifier;
};

constexpr ErrorTest kErrorTests[]{
    {absl::StatusCode::kCancelled, absl::CancelledError, absl::IsCancelled},
    {absl::StatusCode::kUnknown, absl::UnknownError, absl::IsUnknown},
    {absl::StatusCode::kInvalidArgument, absl::InvalidArgumentError,
     absl::IsInvalidArgument},
    {absl::StatusCode::kDeadlineExceeded, absl::DeadlineExceededError,
     absl::IsDeadlineExceeded},
    {absl::StatusCode::kNotFound, absl::NotFoundError, absl::IsNotFound},
    {absl::StatusCode::kAlreadyExists, absl::AlreadyExistsError,
     absl::IsAlreadyExists},
    {absl::StatusCode::kPermissionDenied, absl::PermissionDeniedError,
     absl::IsPermissionDenied},
    {absl::StatusCode::kResourceExhausted, absl::ResourceExhaustedError,
     absl::IsResourceExhausted},
    {absl::StatusCode::kFailedPrecondition, absl::FailedPreconditionError,
     absl::IsFailedPrecondition},
    {absl::StatusCode::kAborted, absl::AbortedError, absl::IsAborted},
    {absl::StatusCode::kOutOfRange, absl::OutOfRangeError, absl::IsOutOfRange},
    {absl::StatusCode::kUnimplemented, absl::UnimplementedError,
     absl::IsUnimplemented},
    {absl::StatusCode::kInternal, absl::InternalError, absl::IsInternal},
    {absl::StatusCode::kUnavailable, absl::UnavailableError,
     absl::IsUnavailable},
    {absl::StatusCode::kDataLoss, absl::DataLossError, absl::IsDataLoss},
    {absl::StatusCode::kUnauthenticated, absl::UnauthenticatedError,
     absl::IsUnauthenticated},
};

TEST(Status, CreateAndClassify) {
  for (const auto& test : kErrorTests) {
    SCOPED_TRACE(absl::StatusCodeToString(test.code));

    // Ensure that the creator does, in fact, create status objects with the
    // expected error code and message.
    std::string message =
        absl::StrCat("error code ", test.code, " test message");
    absl::Status status =
        test.creator(message, absl::SourceLocation::current());
    EXPECT_EQ(test.code, status.code());
    EXPECT_EQ(message, status.message());

    // Ensure that the classifier returns true for a status produced by the
    // creator.
    EXPECT_TRUE(test.classifier(status));

    // Ensure that the classifier returns false for status with a different
    // code.
    for (const auto& other : kErrorTests) {
      if (other.code != test.code) {
        EXPECT_FALSE(test.classifier(absl::Status(other.code, "")))
            << " other.code = " << other.code;
      }
    }
  }
}

TEST(Status, DefaultConstructor) {
  absl::Status status;
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(absl::StatusCode::kOk, status.code());
  EXPECT_EQ("", status.message());
}

TEST(Status, OkStatus) {
  absl::Status status = absl::OkStatus();
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(absl::StatusCode::kOk, status.code());
  EXPECT_EQ("", status.message());
}

TEST(Status, ConstructorWithCodeMessage) {
  {
    absl::Status status(absl::StatusCode::kCancelled, "");
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kCancelled, status.code());
    EXPECT_EQ("", status.message());
  }
  {
    absl::Status status(absl::StatusCode::kInternal, "message");
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    EXPECT_EQ("message", status.message());
  }
}

TEST(Status, StatusMessageCStringTest) {
  {
    absl::Status status = absl::OkStatus();
    EXPECT_EQ(status.message(), "");
    EXPECT_STREQ(absl::StatusMessageAsCStr(status), "");
    EXPECT_EQ(status.message(), absl::StatusMessageAsCStr(status));
    EXPECT_NE(absl::StatusMessageAsCStr(status), nullptr);
  }
  {
    absl::Status status;
    EXPECT_EQ(status.message(), "");
    EXPECT_NE(absl::StatusMessageAsCStr(status), nullptr);
    EXPECT_STREQ(absl::StatusMessageAsCStr(status), "");
  }
  {
    absl::Status status(absl::StatusCode::kInternal, "message");
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    EXPECT_EQ("message", status.message());
    EXPECT_STREQ("message", absl::StatusMessageAsCStr(status));
  }
}

TEST(Status, ConstructOutOfRangeCode) {
  const int kRawCode = 9999;
  absl::Status status(static_cast<absl::StatusCode>(kRawCode), "");
  EXPECT_EQ(absl::StatusCode::kUnknown, status.code());
  EXPECT_EQ(kRawCode, status.raw_code());
}

constexpr char kUrl1[] = "url.payload.1";
constexpr char kUrl2[] = "url.payload.2";
constexpr char kUrl3[] = "url.payload.3";
constexpr char kUrl4[] = "url.payload.xx";

constexpr char kPayload1[] = "aaaaa";
constexpr char kPayload2[] = "bbbbb";
constexpr char kPayload3[] = "ccccc";

using PayloadsVec = std::vector<std::pair<std::string, absl::Cord>>;

TEST(Status, TestGetSetPayload) {
  absl::Status ok_status = absl::OkStatus();
  ok_status.SetPayload(kUrl1, absl::Cord(kPayload1));
  ok_status.SetPayload(kUrl2, absl::Cord(kPayload2));

  EXPECT_FALSE(ok_status.GetPayload(kUrl1));
  EXPECT_FALSE(ok_status.GetPayload(kUrl2));

  absl::Status bad_status(absl::StatusCode::kInternal, "fail");
  bad_status.SetPayload(kUrl1, absl::Cord(kPayload1));
  bad_status.SetPayload(kUrl2, absl::Cord(kPayload2));

  EXPECT_THAT(bad_status.GetPayload(kUrl1), Optional(Eq(kPayload1)));
  EXPECT_THAT(bad_status.GetPayload(kUrl2), Optional(Eq(kPayload2)));

  EXPECT_FALSE(bad_status.GetPayload(kUrl3));

  bad_status.SetPayload(kUrl1, absl::Cord(kPayload3));
  EXPECT_THAT(bad_status.GetPayload(kUrl1), Optional(Eq(kPayload3)));

  // Testing dynamically generated type_url
  bad_status.SetPayload(absl::StrCat(kUrl1, ".1"), absl::Cord(kPayload1));
  EXPECT_THAT(bad_status.GetPayload(absl::StrCat(kUrl1, ".1")),
              Optional(Eq(kPayload1)));
}

TEST(Status, TestErasePayload) {
  absl::Status bad_status(absl::StatusCode::kInternal, "fail");
  bad_status.SetPayload(kUrl1, absl::Cord(kPayload1));
  bad_status.SetPayload(kUrl2, absl::Cord(kPayload2));
  bad_status.SetPayload(kUrl3, absl::Cord(kPayload3));

  EXPECT_FALSE(bad_status.ErasePayload(kUrl4));

  EXPECT_TRUE(bad_status.GetPayload(kUrl2));
  EXPECT_TRUE(bad_status.ErasePayload(kUrl2));
  EXPECT_FALSE(bad_status.GetPayload(kUrl2));
  EXPECT_FALSE(bad_status.ErasePayload(kUrl2));

  EXPECT_TRUE(bad_status.ErasePayload(kUrl1));
  EXPECT_TRUE(bad_status.ErasePayload(kUrl3));

  bad_status.SetPayload(kUrl1, absl::Cord(kPayload1));
  EXPECT_TRUE(bad_status.ErasePayload(kUrl1));
}

TEST(Status, TestComparePayloads) {
  absl::Status bad_status1(absl::StatusCode::kInternal, "fail");
  bad_status1.SetPayload(kUrl1, absl::Cord(kPayload1));
  bad_status1.SetPayload(kUrl2, absl::Cord(kPayload2));
  bad_status1.SetPayload(kUrl3, absl::Cord(kPayload3));

  absl::Status bad_status2(absl::StatusCode::kInternal, "fail");
  bad_status2.SetPayload(kUrl2, absl::Cord(kPayload2));
  bad_status2.SetPayload(kUrl3, absl::Cord(kPayload3));
  bad_status2.SetPayload(kUrl1, absl::Cord(kPayload1));

  EXPECT_EQ(bad_status1, bad_status2);
}

TEST(Status, TestComparePayloadsAfterErase) {
  absl::Status payload_status(absl::StatusCode::kInternal, "");
  payload_status.SetPayload(kUrl1, absl::Cord(kPayload1));
  payload_status.SetPayload(kUrl2, absl::Cord(kPayload2));

  absl::Status empty_status(absl::StatusCode::kInternal, "");

  // Different payloads, not equal
  EXPECT_NE(payload_status, empty_status);
  EXPECT_TRUE(payload_status.ErasePayload(kUrl1));

  // Still Different payloads, still not equal.
  EXPECT_NE(payload_status, empty_status);
  EXPECT_TRUE(payload_status.ErasePayload(kUrl2));

  // Both empty payloads, should be equal
  EXPECT_EQ(payload_status, empty_status);
}

PayloadsVec AllVisitedPayloads(const absl::Status& s) {
  PayloadsVec result;

  s.ForEachPayload([&](absl::string_view type_url, const absl::Cord& payload) {
    result.push_back(std::make_pair(std::string(type_url), payload));
  });

  return result;
}

TEST(Status, TestForEachPayload) {
  absl::Status bad_status(absl::StatusCode::kInternal, "fail");
  bad_status.SetPayload(kUrl1, absl::Cord(kPayload1));
  bad_status.SetPayload(kUrl2, absl::Cord(kPayload2));
  bad_status.SetPayload(kUrl3, absl::Cord(kPayload3));

  int count = 0;

  bad_status.ForEachPayload(
      [&count](absl::string_view, const absl::Cord&) { ++count; });

  EXPECT_EQ(count, 3);

  PayloadsVec expected_payloads = {{kUrl1, absl::Cord(kPayload1)},
                                   {kUrl2, absl::Cord(kPayload2)},
                                   {kUrl3, absl::Cord(kPayload3)}};

  // Test that we visit all the payloads in the status.
  PayloadsVec visited_payloads = AllVisitedPayloads(bad_status);
  EXPECT_THAT(visited_payloads, UnorderedElementsAreArray(expected_payloads));

  // Test that visitation order is not consistent between run.
  std::vector<absl::Status> scratch;
  while (true) {
    scratch.emplace_back(absl::StatusCode::kInternal, "fail");

    scratch.back().SetPayload(kUrl1, absl::Cord(kPayload1));
    scratch.back().SetPayload(kUrl2, absl::Cord(kPayload2));
    scratch.back().SetPayload(kUrl3, absl::Cord(kPayload3));

    if (AllVisitedPayloads(scratch.back()) != visited_payloads) {
      break;
    }
  }
}

TEST(Status, ToString) {
  absl::Status status(absl::StatusCode::kInternal, "fail");
  EXPECT_EQ("INTERNAL: fail", status.ToString());
  status.SetPayload("foo", absl::Cord("bar"));
  EXPECT_EQ("INTERNAL: fail [foo='bar']", status.ToString());
  status.SetPayload("bar", absl::Cord("\377"));
  EXPECT_THAT(status.ToString(),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']")));
}

TEST(Status, ToStringMode) {
  absl::Status status(absl::StatusCode::kInternal, "fail");
  status.SetPayload("foo", absl::Cord("bar"));
  status.SetPayload("bar", absl::Cord("\377"));

  EXPECT_EQ("INTERNAL: fail",
            status.ToString(absl::StatusToStringMode::kWithNoExtraData));

  EXPECT_THAT(status.ToString(absl::StatusToStringMode::kWithPayload),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']")));

  EXPECT_THAT(status.ToString(absl::StatusToStringMode::kWithEverything),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']")));

  EXPECT_THAT(status.ToString(~absl::StatusToStringMode::kWithPayload),
              AllOf(HasSubstr("INTERNAL: fail"), Not(HasSubstr("[foo='bar']")),
                    Not(HasSubstr("[bar='\\xff']"))));
}

TEST(Status, OstreamOperator) {
  absl::Status status(absl::StatusCode::kInternal, "fail");
  { std::stringstream stream;
    stream << status;
    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr("INTERNAL: fail"),
                      HasSubstr("status_test.cc:")));
  }
  status.SetPayload("foo", absl::Cord("bar"));
  { std::stringstream stream;
    stream << status;
    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                      HasSubstr("status_test.cc:")));
  }
  status.SetPayload("bar", absl::Cord("\377"));
  { std::stringstream stream;
    stream << status;
    EXPECT_THAT(stream.str(),
                AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                      HasSubstr("[bar='\\xff']"),
                      HasSubstr("status_test.cc:")));
  }
}

TEST(Status, AbslStringify) {
  absl::Status status(absl::StatusCode::kInternal, "fail");
  EXPECT_THAT(absl::StrCat(status),
              AllOf(HasSubstr("INTERNAL: fail"),
                    HasSubstr("status_test.cc:")));
  EXPECT_THAT(absl::StrFormat("%v", status),
              AllOf(HasSubstr("INTERNAL: fail"),
                    HasSubstr("status_test.cc:")));
  EXPECT_EQ(absl::StrCat(status), absl::StrFormat("%v", status));
  status.SetPayload("foo", absl::Cord("bar"));
  EXPECT_THAT(absl::StrCat(status),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("status_test.cc:")));
  status.SetPayload("bar", absl::Cord("\377"));
  EXPECT_THAT(absl::StrCat(status),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']"),
                    HasSubstr("status_test.cc:")));
}

TEST(Status, OstreamEqStringify) {
  absl::Status status(absl::StatusCode::kUnknown, "fail");
  status.SetPayload("foo", absl::Cord("bar"));
  std::stringstream stream;
  stream << status;
  EXPECT_EQ(stream.str(), absl::StrCat(status));
}

absl::Status EraseAndReturn(const absl::Status& base) {
  absl::Status copy = base;
  EXPECT_TRUE(copy.ErasePayload(kUrl1));
  return copy;
}

TEST(Status, CopyOnWriteForErasePayload) {
  {
    absl::Status base(absl::StatusCode::kInvalidArgument, "fail");
    base.SetPayload(kUrl1, absl::Cord(kPayload1));
    EXPECT_TRUE(base.GetPayload(kUrl1).has_value());
    absl::Status copy = EraseAndReturn(base);
    EXPECT_TRUE(base.GetPayload(kUrl1).has_value());
    EXPECT_FALSE(copy.GetPayload(kUrl1).has_value());
  }
  {
    absl::Status base(absl::StatusCode::kInvalidArgument, "fail");
    base.SetPayload(kUrl1, absl::Cord(kPayload1));
    absl::Status copy = base;

    EXPECT_TRUE(base.GetPayload(kUrl1).has_value());
    EXPECT_TRUE(copy.GetPayload(kUrl1).has_value());

    EXPECT_TRUE(base.ErasePayload(kUrl1));

    EXPECT_FALSE(base.GetPayload(kUrl1).has_value());
    EXPECT_TRUE(copy.GetPayload(kUrl1).has_value());
  }
}

TEST(Status, CopyConstructor) {
  {
    absl::Status status;
    absl::Status copy(status);
    EXPECT_EQ(copy, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    absl::Status copy(status);
    EXPECT_EQ(copy, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    status.SetPayload(kUrl1, absl::Cord(kPayload1));
    absl::Status copy(status);
    EXPECT_EQ(copy, status);
  }
}

TEST(Status, CopyAssignment) {
  absl::Status assignee;
  {
    absl::Status status;
    assignee = status;
    EXPECT_EQ(assignee, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    assignee = status;
    EXPECT_EQ(assignee, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    status.SetPayload(kUrl1, absl::Cord(kPayload1));
    assignee = status;
    EXPECT_EQ(assignee, status);
  }
}

TEST(Status, CopyAssignmentIsNotRef) {
  const absl::Status status_orig(absl::StatusCode::kInvalidArgument, "message");
  absl::Status status_copy = status_orig;
  EXPECT_EQ(status_orig, status_copy);
  status_copy.SetPayload(kUrl1, absl::Cord(kPayload1));
  EXPECT_NE(status_orig, status_copy);
}

TEST(Status, MoveConstructor) {
  {
    absl::Status status;
    absl::Status copy(absl::Status{});
    EXPECT_EQ(copy, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    absl::Status copy(
        absl::Status(absl::StatusCode::kInvalidArgument, "message"));
    EXPECT_EQ(copy, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    status.SetPayload(kUrl1, absl::Cord(kPayload1));
    absl::Status copy1(status);
    absl::Status copy2(std::move(status));
    EXPECT_EQ(copy1, copy2);
  }
}

TEST(Status, MoveAssignment) {
  absl::Status assignee;
  {
    absl::Status status;
    assignee = absl::Status();
    EXPECT_EQ(assignee, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    assignee = absl::Status(absl::StatusCode::kInvalidArgument, "message");
    EXPECT_EQ(assignee, status);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    status.SetPayload(kUrl1, absl::Cord(kPayload1));
    absl::Status copy(status);
    assignee = std::move(status);
    EXPECT_EQ(assignee, copy);
  }
  {
    absl::Status status(absl::StatusCode::kInvalidArgument, "message");
    absl::Status copy(status);
    assignee = static_cast<absl::Status&&>(status);
    EXPECT_EQ(assignee, copy);
  }
}

TEST(Status, Update) {
  absl::Status s;
  s.Update(absl::OkStatus());
  EXPECT_TRUE(s.ok());
  const absl::Status a(absl::StatusCode::kCancelled, "message");
  s.Update(a);
  EXPECT_EQ(s, a);
  const absl::Status b(absl::StatusCode::kInternal, "other message");
  s.Update(b);
  EXPECT_EQ(s, a);
  s.Update(absl::OkStatus());
  EXPECT_EQ(s, a);
  EXPECT_FALSE(s.ok());
}

TEST(Status, Equality) {
  absl::Status ok;
  absl::Status no_payload = absl::CancelledError("no payload");
  absl::Status one_payload = absl::InvalidArgumentError("one payload");
  one_payload.SetPayload(kUrl1, absl::Cord(kPayload1));
  absl::Status two_payloads = one_payload;
  two_payloads.SetPayload(kUrl2, absl::Cord(kPayload2));
  const std::array<absl::Status, 4> status_arr = {ok, no_payload, one_payload,
                                                  two_payloads};
  for (int i = 0; i < status_arr.size(); i++) {
    for (int j = 0; j < status_arr.size(); j++) {
      if (i == j) {
        EXPECT_TRUE(status_arr[i] == status_arr[j]);
        EXPECT_FALSE(status_arr[i] != status_arr[j]);
      } else {
        EXPECT_TRUE(status_arr[i] != status_arr[j]);
        EXPECT_FALSE(status_arr[i] == status_arr[j]);
      }
    }
  }
}

TEST(Status, Swap) {
  auto test_swap = [](const absl::Status& s1, const absl::Status& s2) {
    absl::Status copy1 = s1, copy2 = s2;
    swap(copy1, copy2);
    EXPECT_EQ(copy1, s2);
    EXPECT_EQ(copy2, s1);
  };
  const absl::Status ok;
  const absl::Status no_payload(absl::StatusCode::kAlreadyExists, "no payload");
  absl::Status with_payload(absl::StatusCode::kInternal, "with payload");
  with_payload.SetPayload(kUrl1, absl::Cord(kPayload1));
  test_swap(ok, no_payload);
  test_swap(no_payload, ok);
  test_swap(ok, with_payload);
  test_swap(with_payload, ok);
  test_swap(no_payload, with_payload);
  test_swap(with_payload, no_payload);
}

TEST(StatusErrno, ErrnoToStatusCode) {
  EXPECT_EQ(absl::ErrnoToStatusCode(0), absl::StatusCode::kOk);

  // Spot-check a few errno values.
  EXPECT_EQ(absl::ErrnoToStatusCode(EINVAL),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(absl::ErrnoToStatusCode(ENOENT), absl::StatusCode::kNotFound);

  // We'll pick a very large number so it hopefully doesn't collide to errno.
  EXPECT_EQ(absl::ErrnoToStatusCode(19980927), absl::StatusCode::kUnknown);
}

TEST(StatusErrno, ErrnoToStatus) {
  absl::Status status = absl::ErrnoToStatus(ENOENT, "Cannot open 'path'");
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(status.message(), "Cannot open 'path': No such file or directory");
}

#ifdef ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE
#define GET_SOURCE_LOCATION(offset) __builtin_LINE() - offset
#else
#define GET_SOURCE_LOCATION(offset) 1
#endif

void CheckSourceLocation(
    const absl::Status& status, std::vector<int> lines = {},
    absl::SourceLocation loc = absl::SourceLocation::current()) {
  ASSERT_EQ(status.GetSourceLocations().size(), lines.size())
      << "Size check failed at " << loc.line();
  for (size_t i = 0; i < lines.size(); ++i) {
    EXPECT_EQ(absl::string_view(status.GetSourceLocations()[i].file_name()),
              absl::string_view(loc.file_name()))
        << "File name check failed at " << loc.line();
    EXPECT_EQ(status.GetSourceLocations()[i].line(), lines[i])
        << "Line check failed at " << loc.line();
  }
}

TEST(Status, ConstructorCheckSourceLocation) {
  {
    const absl::Status a;
    const absl::Status b = a;
    for (const absl::Status& status : {a, b}) {
      EXPECT_TRUE(status.ok());
      EXPECT_EQ(absl::StatusCode::kOk, status.code());
      CheckSourceLocation(status);
    }
  }
  {
    const absl::Status a(absl::StatusCode::kInternal, "message",
                         absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    const absl::Status b = a;
    for (const absl::Status& status : {a, b}) {
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(absl::StatusCode::kInternal, status.code());
      CheckSourceLocation(status, {line});
    }
  }
  {
    const absl::Status a(absl::StatusCode::kInternal, "message",
                         absl::SourceLocation());
    const absl::Status b = a;
    for (const absl::Status& status : {a, b}) {
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(absl::StatusCode::kInternal, status.code());
      CheckSourceLocation(status);
    }
  }
  {
    const absl::Status a(absl::StatusCode::kInternal, "",
                         absl::SourceLocation::current());
    const absl::Status b = a;
    for (const absl::Status& status : {a, b}) {
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(absl::StatusCode::kInternal, status.code());
      CheckSourceLocation(status);
    }
  }
  {
    const absl::Status a(absl::StatusCode::kInternal, "",
                         absl::SourceLocation());
    const absl::Status b = a;
    for (const absl::Status& status : {a, b}) {
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(absl::StatusCode::kInternal, status.code());
      CheckSourceLocation(status);
    }
  }
}

TEST(Status, SourceLocationConstructor) {
  {
    // OK status doesn't save source locations.
    const absl::Status original;
    const absl::Status status(original, absl::SourceLocation());
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(absl::StatusCode::kOk, status.code());
    CheckSourceLocation(status);
  }
  {
    // OK status doesn't save source locations.
    const absl::Status original;
    const absl::Status status(original, absl::SourceLocation::current());
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(absl::StatusCode::kOk, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-ok Status with non-empty msg can save source locations with
    // non-nullptr filename.
    const absl::Status original(absl::StatusCode::kInternal, "message",
                                absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    // Default absl::SourceLocation cannot be saved into the chain.
    const absl::Status status(original, absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status, {line});
  }
  {
    const absl::Status original(absl::StatusCode::kInternal, "message",
                                absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);

    const absl::Status status(original, absl::SourceLocation::current());
    int line2 = GET_SOURCE_LOCATION(1);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status, {line, line2});
  }
  {
    // Non-OK status with empty msg doesn't save source locations.
    const absl::Status original(absl::StatusCode::kInternal, "",
                                absl::SourceLocation::current());
    const absl::Status status(original, absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-OK status with empty msg doesn't save source locations.
    const absl::Status original(absl::StatusCode::kInternal, "",
                                absl::SourceLocation::current());
    const absl::Status status(original, absl::SourceLocation::current());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-OK status with empty msg doesn't save source locations from default
    // constructor.
    const absl::Status original(absl::StatusCode::kInternal, "",
                                absl::SourceLocation());
    const absl::Status status(original, absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-OK status with empty msg doesn't save source locations.
    const absl::Status original(absl::StatusCode::kInternal, "",
                                absl::SourceLocation());
    const absl::Status status(original, absl::SourceLocation::current());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    const absl::Status original(absl::StatusCode::kInternal, "message",
                                absl::SourceLocation());
    const absl::Status status(original, absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    const absl::Status original(absl::StatusCode::kInternal, "message",
                                absl::SourceLocation());
    const absl::Status status(original, absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status, {line});
  }
}

TEST(Status, SourceLocationWithMoveConstructor) {
  {
    // OK status doesn't save source locations.
    absl::Status original;
    const absl::Status status(std::move(original), absl::SourceLocation());
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(absl::StatusCode::kOk, status.code());
    CheckSourceLocation(status);
  }
  {
    // OK status doesn't save source locations.
    absl::Status original;
    const absl::Status status(std::move(original),
                              absl::SourceLocation::current());
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(absl::StatusCode::kOk, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-ok Status with non-empty msg can save source locations with
    // non-nullptr filename.
    absl::Status original(absl::StatusCode::kInternal, "message",
                          absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    // Default absl::SourceLocation cannot be saved into the chain.
    const absl::Status status(std::move(original), absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status, {line});
  }
  {
    absl::Status original(absl::StatusCode::kInternal, "message",
                          absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);

    const absl::Status status(std::move(original),
                              absl::SourceLocation::current());
    int line2 = GET_SOURCE_LOCATION(1);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status, {line, line2});
  }
  {
    // Non-OK status with empty msg doesn't save source locations.
    absl::Status original(absl::StatusCode::kInternal, "",
                          absl::SourceLocation::current());
    const absl::Status status(std::move(original), absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-OK status with empty msg doesn't save source locations.
    absl::Status original(absl::StatusCode::kInternal, "",
                          absl::SourceLocation::current());
    const absl::Status status(std::move(original),
                              absl::SourceLocation::current());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-OK status with empty msg doesn't save source locations from default
    // constructor.
    absl::Status original(absl::StatusCode::kInternal, "",
                          absl::SourceLocation());
    const absl::Status status(std::move(original), absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    // Non-OK status with empty msg doesn't save source locations.
    absl::Status original(absl::StatusCode::kInternal, "",
                          absl::SourceLocation());
    const absl::Status status(std::move(original),
                              absl::SourceLocation::current());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    absl::Status original(absl::StatusCode::kInternal, "message",
                          absl::SourceLocation());
    const absl::Status status(std::move(original), absl::SourceLocation());
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status);
  }
  {
    absl::Status original(absl::StatusCode::kInternal, "message",
                          absl::SourceLocation());
    const absl::Status status(std::move(original),
                              absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(absl::StatusCode::kInternal, status.code());
    CheckSourceLocation(status, {line});
  }
}

TEST(Status, AddSourceLocation) {
  int max_iter = 10;
  {
    // Status that ignores source location.
    absl::Status status_ignores_source_location[] = {
        absl::Status(),
        absl::Status(absl::StatusCode::kInternal, "")};
    for (absl::Status& s : status_ignores_source_location) {
      for (int i = 0; i < max_iter; ++i) {
        s.AddSourceLocation(absl::SourceLocation::current());
        s.AddSourceLocation(absl::SourceLocation());
      }
      CheckSourceLocation(s);
    }
  }
  {
    // Default SourceLocation is not added.
    absl::Status status(absl::StatusCode::kInternal, "foo",
                        absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    for (int i = 0; i < max_iter; ++i) {
      status.AddSourceLocation(absl::SourceLocation());
    }
    CheckSourceLocation(status, {line});
  }
  {
    // Default SourceLocation is not added.
    absl::Status status(absl::StatusCode::kInternal, "foo",
                        absl::SourceLocation::current());
    int line = GET_SOURCE_LOCATION(1);
    std::vector<int> lines = {line};
    lines.reserve(1 + max_iter);
    for (int i = 0; i < max_iter; ++i) {
      status.AddSourceLocation(absl::SourceLocation::current());
      lines.push_back(GET_SOURCE_LOCATION(1));
    }
    CheckSourceLocation(status, lines);
  }
}

TEST(Status, WithSourceLocationCopy) {
  absl::Status original(absl::StatusCode::kInternal, "message",
                        absl::SourceLocation::current());
  int line = GET_SOURCE_LOCATION(1);

  const absl::Status status =
      original.WithSourceLocation(absl::SourceLocation::current());
  int line2 = GET_SOURCE_LOCATION(1);

  CheckSourceLocation(original, {line});
  CheckSourceLocation(status, {line, line2});
  EXPECT_EQ(original, status);
}

absl::Status&& IsRvalueStatus(absl::Status&& s) { return std::move(s); }

TEST(Status, WithSourceLocationMove) {
  absl::Status original(absl::StatusCode::kInternal, "message",
                        absl::SourceLocation::current());
  int line = GET_SOURCE_LOCATION(1);

  const absl::Status status = IsRvalueStatus(
      std::move(original).WithSourceLocation(absl::SourceLocation::current()));
  int line2 = GET_SOURCE_LOCATION(1);

  CheckSourceLocation(status, {line, line2});
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(absl::StatusCode::kInternal, status.code());
}

TEST(Status, CopyOnWriteSourceLocations) {
  absl::Status source(absl::StatusCode::kInvalidArgument, "fail",
                      absl::SourceLocation::current());
  EXPECT_EQ(source.GetSourceLocations().size(), 1);
  absl::Status copy = source;
  EXPECT_EQ(copy.GetSourceLocations().size(), 1);
  copy.AddSourceLocation(absl::SourceLocation::current());  // Copy rep.
  EXPECT_EQ(copy.GetSourceLocations().size(), 2);
  EXPECT_EQ(source.GetSourceLocations().size(), 1);
}

TEST(Status, SourceLocationToStringMode) {
  absl::Status s(absl::StatusCode::kInternal, "fail",
                 absl::SourceLocation::current());
  int line = GET_SOURCE_LOCATION(1);
  std::string source_location_string = "\n=== Source Location Trace: ===";
  std::string source_location_stack = absl::StrCat(
      absl::SourceLocation::current().file_name(), ":", line, "\n");

  s.SetPayload("foo", absl::Cord("bar"));

  EXPECT_EQ("INTERNAL: fail",
            s.ToString(absl::StatusToStringMode::kWithNoExtraData));

  EXPECT_EQ("INTERNAL: fail",
            s.ToString(~absl::StatusToStringMode::kWithSourceLocation &
                       ~absl::StatusToStringMode::kWithPayload));
  EXPECT_THAT(s.ToString(absl::StatusToStringMode::kWithSourceLocation |
                         absl::StatusToStringMode::kWithPayload),
              AllOf(HasSubstr("INTERNAL: fail [foo='bar']"),
                    HasSubstr(source_location_string),
                    HasSubstr(source_location_stack)));

  s.SetPayload("bar", absl::Cord("\377"));

  EXPECT_THAT(s.ToString(absl::StatusToStringMode::kWithEverything),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']"),
                    HasSubstr(source_location_string),
                    HasSubstr(source_location_stack)));
  EXPECT_THAT(s.ToString(absl::StatusToStringMode::kWithPayload |
                         absl::StatusToStringMode::kWithSourceLocation),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']"),
                    HasSubstr(source_location_string),
                    HasSubstr(source_location_stack)));
  EXPECT_THAT(s.ToString(absl::StatusToStringMode::kWithSourceLocation),
              AllOf(HasSubstr("INTERNAL: fail"), Not(HasSubstr("[foo='bar']")),
                    Not(HasSubstr("[bar='\\xff']")),
                    HasSubstr(source_location_string),
                    HasSubstr(source_location_stack)));
  EXPECT_THAT(s.ToString(absl::StatusToStringMode::kWithPayload),
              AllOf(HasSubstr("INTERNAL: fail"), HasSubstr("[foo='bar']"),
                    HasSubstr("[bar='\\xff']"),
                    Not(HasSubstr(source_location_string)),
                    Not(HasSubstr(source_location_stack))));
}

TEST(Status, StackTracePayloadOverflow) {
  // Stack must have the same layout as status_internal::StackTracePayload.
  struct Stack {
    size_t size;
    void* frames[20];
  } stack;
  stack.size = 200;  // Overflows frames.

  absl::Status status = absl::CancelledError();
  status.SetPayload("AbslStatusStackTracePayload",
                    absl::Cord(absl::string_view(
                        reinterpret_cast<const char*>(&stack), sizeof(stack))));

  // An unchecked overflow should be detected by ASAN/MSAN on the next line.
  static_cast<void>(status.ToString(absl::StatusToStringMode::kWithEverything));
}

}  // namespace
