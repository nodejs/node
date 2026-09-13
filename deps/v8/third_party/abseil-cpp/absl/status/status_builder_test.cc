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

#include <stddef.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace {

using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;

// Converts a StatusBuilder to a Status.
absl::Status ToStatus(const StatusBuilder& s) { return s; }

// Converts a StatusBuilder to a StatusOr<T>.
template <typename T>
absl::StatusOr<T> ToStatusOr(const StatusBuilder& s) {
  return s;
}

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

class StatusBuilderTest : public ::testing::Test {};

TEST_F(StatusBuilderTest, Size) {
  EXPECT_LE(sizeof(StatusBuilder), 40)
      << "Relax this test with caution and thorough testing. If StatusBuilder "
         "is too large it can potentially blow stacks, especially in debug "
         "builds. See the comments for StatusBuilder::Rep.";
}

TEST_F(StatusBuilderTest, ExplicitSourceLocation) {
  const absl::SourceLocation kLocation = absl::SourceLocation::current();

  {
    const StatusBuilder builder(absl::OkStatus(), kLocation);
    EXPECT_THAT(builder.source_location().file_name(),
                Eq(kLocation.file_name()));
    EXPECT_THAT(builder.source_location().line(), Eq(kLocation.line()));
  }
}

TEST_F(StatusBuilderTest, ImplicitSourceLocation) {
  const StatusBuilder builder(absl::OkStatus());
  auto loc = absl::SourceLocation::current();
  EXPECT_THAT(builder.source_location().file_name(),
              AnyOf(Eq(absl::string_view(loc.file_name())),
                    Eq(absl::string_view("<source_location>"))));
  EXPECT_THAT(builder.source_location().line(),
              AnyOf(Eq(1), Eq(loc.line() - 1)));
}

testing::Matcher<absl::SourceLocation> SourceLocationIs(
    absl::SourceLocation loc) {
  return AnyOf(
      AllOf(Property(&absl::SourceLocation::file_name, Eq(loc.file_name())),
            Property(&absl::SourceLocation::line, Eq(loc.line()))),
      // Fallback for platforms that don't support source locations.
      AllOf(Property(&absl::SourceLocation::file_name, Eq("<source_location>")),
            Property(&absl::SourceLocation::line, Eq(1))));
}

TEST_F(StatusBuilderTest, GetPreviousSourceLocations) {
  const absl::SourceLocation loc0 = absl::SourceLocation::current();
  absl::Status status = absl::InvalidArgumentError("hi", loc0);
  const absl::SourceLocation loc1 = absl::SourceLocation::current();
  status.AddSourceLocation(loc1);
  const absl::SourceLocation loc2 = absl::SourceLocation::current();
  status.AddSourceLocation(loc2);

  // The builder's location is not included.
  const StatusBuilder builder(status);
  EXPECT_THAT(builder.GetPreviousSourceLocations(),
              ElementsAre(SourceLocationIs(loc0), SourceLocationIs(loc1),
                          SourceLocationIs(loc2)));
}

TEST_F(StatusBuilderTest, EmptyGetPreviousSourceLocationsForNewFromStatusCode) {
  const StatusBuilder builder(absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(builder.GetPreviousSourceLocations(), IsEmpty());
}

TEST_F(StatusBuilderTest, StatusCode) {
  // OK
  {
    const StatusBuilder builder(absl::StatusCode::kOk);
    EXPECT_TRUE(builder.ok());
    EXPECT_THAT(builder.code(), Eq(absl::StatusCode::kOk));
  }
  // Non-OK code
  {
    const StatusBuilder builder(absl::StatusCode::kInvalidArgument);
    EXPECT_FALSE(builder.ok());
    EXPECT_THAT(builder.code(), Eq(absl::StatusCode::kInvalidArgument));
  }
}

TEST_F(StatusBuilderTest, OkIgnoresStuff) {
  EXPECT_THAT(ToStatus(StatusBuilder(absl::OkStatus(), absl::SourceLocation())
                       << "booyah"),
              Eq(absl::OkStatus()));
}

TEST_F(StatusBuilderTest, Streaming) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::CancelledError(), absl::SourceLocation())
               << "booyah"),
      Eq(absl::CancelledError("booyah")));
  EXPECT_THAT(
      ToStatus(
          StatusBuilder(absl::AbortedError("hello"), absl::SourceLocation())
          << "world"),
      Eq(absl::AbortedError("hello; world")));
}

TEST_F(StatusBuilderTest, PrependLvalue) {
  {
    StatusBuilder builder(absl::CancelledError(), absl::SourceLocation());
    EXPECT_THAT(ToStatus(builder.SetPrepend() << "booyah"),
                Eq(absl::CancelledError("booyah")));
  }
  {
    StatusBuilder builder(absl::AbortedError(" hello"), absl::SourceLocation());
    EXPECT_THAT(ToStatus(builder.SetPrepend() << "world"),
                Eq(absl::AbortedError("world hello")));
  }
}

TEST_F(StatusBuilderTest, PrependRvalue) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::CancelledError(), absl::SourceLocation())
                   .SetPrepend()
               << "booyah"),
      Eq(absl::CancelledError("booyah")));
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError(" hello"),
                                     absl::SourceLocation())
                           .SetPrepend()
                       << "world"),
              Eq(absl::AbortedError("world hello")));
}

TEST_F(StatusBuilderTest, AppendLvalue) {
  {
    StatusBuilder builder(absl::CancelledError(), absl::SourceLocation());
    EXPECT_THAT(ToStatus(builder.SetAppend() << "booyah"),
                Eq(absl::CancelledError("booyah")));
  }
  {
    StatusBuilder builder(absl::AbortedError("hello"), absl::SourceLocation());
    EXPECT_THAT(ToStatus(builder.SetAppend() << " world"),
                Eq(absl::AbortedError("hello world")));
  }
}

TEST_F(StatusBuilderTest, AppendRvalue) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::CancelledError(), absl::SourceLocation())
                   .SetAppend()
               << "booyah"),
      Eq(absl::CancelledError("booyah")));
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .SetAppend()
                       << " world"),
              Eq(absl::AbortedError("hello world")));
}

TEST_F(StatusBuilderTest, WithRvalueRef) {
  auto policy = [](StatusBuilder sb) { return sb << "policy"; };
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .With(policy)),
              Eq(absl::AbortedError("hello; policy")));
}

TEST_F(StatusBuilderTest, WithRef) {
  auto policy = [](StatusBuilder sb) { return sb << "policy"; };
  StatusBuilder sb(absl::AbortedError("zomg"), absl::SourceLocation());
  EXPECT_THAT(ToStatus(sb.With(policy)),
              Eq(absl::AbortedError("zomg; policy")));
}

TEST_F(StatusBuilderTest, WithTypeChange) {
  auto policy = [](StatusBuilder sb) -> std::string {
    return sb.ok() ? "true" : "false";
  };
  EXPECT_EQ(StatusBuilder(absl::CancelledError(), absl::SourceLocation())
                .With(policy),
            "false");
  EXPECT_EQ(
      StatusBuilder(absl::OkStatus(), absl::SourceLocation()).With(policy),
      "true");
}

struct MoveOnlyAdaptor {
  std::unique_ptr<int> value;
  std::unique_ptr<int> operator()(const absl::Status&) && {
    return std::move(value);
  }
};

TEST_F(StatusBuilderTest, WithMoveOnlyAdaptor) {
  StatusBuilder sb(absl::AbortedError("zomg"), absl::SourceLocation());
  EXPECT_THAT(sb.With(MoveOnlyAdaptor{std::make_unique<int>(100)}),
              Pointee(100));
  EXPECT_THAT(StatusBuilder(absl::AbortedError("zomg"), absl::SourceLocation())
                  .With(MoveOnlyAdaptor{std::make_unique<int>(100)}),
              Pointee(100));
}

struct StringifiableType {
  absl::string_view message;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const StringifiableType& o) {
    sink.Append(o.message);
  }
};

class MockLogSink : public absl::LogSink {
 public:
  MOCK_METHOD(void, Send, (const absl::LogEntry&), (override));
};

TEST(WithExtraMessagePolicyTest, AppendsToExtraMessage) {
  // The policy simply calls operator<< on the builder; the following examples
  // demonstrate that, without duplicating all of the above tests.
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .With(ExtraMessage("world"))),
              Eq(absl::AbortedError("hello; world")));
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .With(ExtraMessage() << "world")),
              Eq(absl::AbortedError("hello; world")));
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .With(ExtraMessage("world"))
                           .With(ExtraMessage("!"))),
              Eq(absl::AbortedError("hello; world!")));
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .With(ExtraMessage("world, "))
                           .SetPrepend()),
              Eq(absl::AbortedError("world, hello")));
  EXPECT_THAT(ToStatus(StatusBuilder(absl::AbortedError("hello"),
                                     absl::SourceLocation())
                           .With(ExtraMessage() << StringifiableType{"world"})),
              Eq(absl::AbortedError("hello; world")));

  // The above examples use temporary StatusBuilder rvalues; verify things also
  // work fine when StatusBuilder is an lvalue.
  StatusBuilder builder(absl::AbortedError("hello"), absl::SourceLocation());
  EXPECT_THAT(
      ToStatus(builder.With(ExtraMessage("world")).With(ExtraMessage("!"))),
      Eq(absl::AbortedError("hello; world!")));
}

TEST(WithExtraMessagePolicyTest,
     ExtraMessageStreamOperatorPreservesRvalueness) {
  static_assert(
      std::is_same_v<ExtraMessage&&, decltype(ExtraMessage() << "foo")>);
}

TEST_F(StatusBuilderTest, StatusSourceLocationChaining) {
  {
    absl::Status src = absl::OkStatus();
    CheckSourceLocation(src);
    CheckSourceLocation(ToStatus(StatusBuilder(src, absl::SourceLocation())));
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current())));
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current()) << "hmm"));
  }
  {
    absl::Status src = absl::Status(absl::StatusCode::kCancelled, "");
    CheckSourceLocation(src);
    CheckSourceLocation(ToStatus(StatusBuilder(src, absl::SourceLocation())));
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current())));
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current()) << ""));
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current()) << "hmm"),
        {__builtin_LINE() - 1});
  }
  {
    absl::Status src = absl::Status(absl::StatusCode::kCancelled, "msg",
                                    absl::SourceLocation());
    CheckSourceLocation(src);
    CheckSourceLocation(ToStatus(StatusBuilder(src, absl::SourceLocation())));
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current())),
        {__builtin_LINE() - 1});
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current()) << "hmm"),
        {__builtin_LINE() - 1});
  }
  {
    absl::Status src = absl::Status(absl::StatusCode::kCancelled, "msg");
    int src_line = __builtin_LINE() - 1;
    CheckSourceLocation(src, {src_line});
    CheckSourceLocation(ToStatus(StatusBuilder(src, absl::SourceLocation())),
                        {src_line});
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current())),
        {src_line, __builtin_LINE() - 1});
    CheckSourceLocation(
        ToStatus(StatusBuilder(src, absl::SourceLocation::current()) << "hmm"),
        {src_line, __builtin_LINE() - 1});
  }
}

TEST_F(StatusBuilderTest, SetErrorCode) {
  StatusBuilder builder;
  builder.SetCode(absl::StatusCode::kResourceExhausted);
  LOG(INFO) << "Builder code: " << builder;
  EXPECT_FALSE(builder.ok());
  EXPECT_EQ(builder.code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(StatusBuilderTest, BuilderToStatusOrStatusShouldGiveErrorStatusOr) {
  absl::StatusOr<absl::Status> value = StatusBuilder(absl::CancelledError());
  ASSERT_FALSE(value.ok());
  EXPECT_THAT(value.status(), StatusIs(absl::StatusCode::kCancelled));
}

}  // namespace
ABSL_NAMESPACE_END
}  // namespace absl
