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

#include "absl/status/status_macros.h"

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/status_builder.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::HasSubstr;

using absl::status_macro_internal::ReturnIfErrorAdaptor;
using absl::status_macro_internal::StatusAdaptorForMacros;
static_assert(!std::is_default_constructible_v<ReturnIfErrorAdaptor>);
static_assert(!std::is_default_constructible_v<StatusAdaptorForMacros>);
static_assert(!std::is_copy_constructible_v<ReturnIfErrorAdaptor>);
static_assert(!std::is_copy_constructible_v<StatusAdaptorForMacros>);
static_assert(!std::is_copy_assignable_v<ReturnIfErrorAdaptor>);
static_assert(!std::is_copy_assignable_v<StatusAdaptorForMacros>);

absl::Status ReturnOk() { return absl::OkStatus(); }

absl::StatusBuilder ReturnOkBuilder() {
  return absl::StatusBuilder(absl::OkStatus());
}

absl::Status ReturnError(absl::string_view msg) {
  return absl::Status(absl::StatusCode::kUnknown, msg);
}

absl::StatusBuilder ReturnErrorBuilder(absl::string_view msg) {
  return absl::StatusBuilder(absl::Status(absl::StatusCode::kUnknown, msg));
}

absl::StatusOr<int> ReturnStatusOrValue(int v) { return v; }

absl::StatusOr<int> ReturnStatusOrError(absl::string_view msg) {
  return absl::Status(absl::StatusCode::kUnknown, msg);
}

template <class... Args>
absl::StatusOr<std::tuple<Args...>> ReturnStatusOrTupleValue(Args&&... v) {
  return std::tuple<Args...>(std::forward<Args>(v)...);
}

template <class... Args>
absl::StatusOr<std::tuple<Args...>> ReturnStatusOrTupleError(
    absl::string_view msg) {
  return absl::Status(absl::StatusCode::kUnknown, msg);
}

absl::StatusOr<int&> ReturnStatusOrRef(int& v) { return v; }

absl::StatusOr<std::unique_ptr<int>> ReturnStatusOrPtrValue(int v) {
  return std::make_unique<int>(v);
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
TEST(AssignOrReturn, Works) {
  auto func = []() -> absl::Status {
    ABSL_ASSIGN_OR_RETURN(int value1, ReturnStatusOrValue(1));
    EXPECT_EQ(1, value1);
    ABSL_ASSIGN_OR_RETURN(const int value2, ReturnStatusOrValue(2));
    EXPECT_EQ(2, value2);
    ABSL_ASSIGN_OR_RETURN(const int& value3, ReturnStatusOrValue(3));
    EXPECT_EQ(3, value3);
    ABSL_ASSIGN_OR_RETURN(int value4 [[maybe_unused]],
                          ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, WorksWithReferences) {
  int value = 17;
  auto func = [&]() -> absl::Status {
    ABSL_ASSIGN_OR_RETURN(int& value1, ReturnStatusOrRef(value));
    EXPECT_EQ(&value1, &value);

    ABSL_ASSIGN_OR_RETURN(const int& value2, ReturnStatusOrRef(value));
    EXPECT_EQ(&value2, &value);

    ABSL_ASSIGN_OR_RETURN(int value3, ReturnStatusOrRef(value));
    EXPECT_EQ(value3, value);

    value = 11;
    EXPECT_NE(value3, value);

    ABSL_ASSIGN_OR_RETURN(int value4 [[maybe_unused]],
                          ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, WorksWithCommasInType) {
  auto func = []() -> absl::Status {
    ABSL_ASSIGN_OR_RETURN((std::tuple<int, int> t1),
                          ReturnStatusOrTupleValue(1, 1));
    EXPECT_EQ((std::tuple{1, 1}), t1);
    ABSL_ASSIGN_OR_RETURN((const std::tuple<int, std::tuple<int, int>, int> t2),
                          ReturnStatusOrTupleValue(1, std::tuple{1, 1}, 1));
    EXPECT_EQ((std::tuple{1, std::tuple{1, 1}, 1}), t2);
    ABSL_ASSIGN_OR_RETURN(
        (std::tuple<int, std::tuple<int, int>, int> t3),
        (ReturnStatusOrTupleError<int, std::tuple<int, int>, int>("EXPECTED")));
    t3 = {};  // fix unused error
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, WorksWithStructureBindings) {
  auto func = []() -> absl::Status {
    ABSL_ASSIGN_OR_RETURN(
        (const auto& [t1, t2, t3, t4, t5]),
        ReturnStatusOrTupleValue(std::tuple{1, 1}, 1, 2, 3, 4));
    EXPECT_EQ((std::tuple{1, 1}), t1);
    EXPECT_EQ(1, t2);
    EXPECT_EQ(2, t3);
    EXPECT_EQ(3, t4);
    EXPECT_EQ(4, t5);
    ABSL_ASSIGN_OR_RETURN(int t6 [[maybe_unused]],
                          ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, WorksWithParenthesesAndDereference) {
  auto func = []() -> absl::Status {
    int integer;
    int* pointer_to_integer = &integer;
    ABSL_ASSIGN_OR_RETURN((*pointer_to_integer), ReturnStatusOrValue(1));
    EXPECT_EQ(1, integer);
    ABSL_ASSIGN_OR_RETURN(*pointer_to_integer, ReturnStatusOrValue(2));
    EXPECT_EQ(2, integer);
    // Make the test where the order of dereference matters and treat the
    // parentheses.
    pointer_to_integer--;
    int** pointer_to_pointer_to_integer = &pointer_to_integer;
    ABSL_ASSIGN_OR_RETURN((*pointer_to_pointer_to_integer)[1],
                          ReturnStatusOrValue(3));
    EXPECT_EQ(3, integer);
    ABSL_ASSIGN_OR_RETURN(int t1 [[maybe_unused]],
                          ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, WorksWithAppend) {
  auto fail_test_if_called = []() -> std::string {
    ADD_FAILURE();
    return "FAILURE";
  };
  auto func = [&]() -> absl::Status {
    int value [[maybe_unused]];
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(1),
                          _ << fail_test_if_called());
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED A"),
                          _ << "EXPECTED B");
    return ReturnOk();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksWithAdaptorFunc) {
  auto fail_test_if_called = [](absl::StatusBuilder builder) {
    ADD_FAILURE();
    return builder;
  };
  auto adaptor = [](absl::StatusBuilder builder) {
    return builder << "EXPECTED B";
  };
  auto func = [&]() -> absl::Status {
    int value [[maybe_unused]];
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(1),
                          fail_test_if_called(_));
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED A"), adaptor(_));
    return ReturnOk();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksWithThirdArgumentAndCommas) {
  auto fail_test_if_called = [](absl::StatusBuilder builder) {
    ADD_FAILURE();
    return builder;
  };
  auto adaptor = [](absl::StatusBuilder builder) {
    return builder << "EXPECTED B";
  };
  auto func = [&]() -> absl::Status {
    ABSL_ASSIGN_OR_RETURN((const auto& [t1, t2, t3]),
                          ReturnStatusOrTupleValue(1, 2, 3),
                          fail_test_if_called(_));
    EXPECT_EQ(t1, 1);
    EXPECT_EQ(t2, 2);
    EXPECT_EQ(t3, 3);
    ABSL_ASSIGN_OR_RETURN(
        (const auto& [t4, t5, t6]),
        (ReturnStatusOrTupleError<int, int, int>("EXPECTED A")), adaptor(_));
    // Silence errors about the unused values.
    static_cast<void>(t4);
    static_cast<void>(t5);
    static_cast<void>(t6);
    return ReturnOk();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksWithAppendIncludingLocals) {
  auto func = [&](absl::string_view str) -> absl::Status {
    int value [[maybe_unused]];
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED A"), _ << str);
    return ReturnOk();
  };

  EXPECT_THAT(func("EXPECTED B").message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksForExistingVariable) {
  auto func = []() -> absl::Status {
    int value = 1;
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(2));
    EXPECT_EQ(2, value);
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(3));
    EXPECT_EQ(3, value);
    ABSL_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, UniquePtrWorks) {
  auto func = []() -> absl::Status {
    ABSL_ASSIGN_OR_RETURN(std::unique_ptr<int> ptr, ReturnStatusOrPtrValue(1));
    EXPECT_EQ(*ptr, 1);
    return ReturnError("EXPECTED");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, UniquePtrWorksForExistingVariable) {
  auto func = []() -> absl::Status {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(1));
    EXPECT_EQ(*ptr, 1);

    ABSL_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(2));
    EXPECT_EQ(*ptr, 2);
    return ReturnError("EXPECTED");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, ChainSourceLocation) {
  auto func1 = []() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(1));
    return ptr;
  };
  auto func2 = []() -> absl::StatusOr<std::unique_ptr<int>> {
    return absl::Status(absl::StatusCode::kInternal, "msg");
  };
  int func2_line = __builtin_LINE() - 2;

  auto func3 = [=]() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, func1());
    ABSL_ASSIGN_OR_RETURN(ptr, func2());
    return ptr;
  };
  int func3_line = __builtin_LINE() - 3;

  auto func4 = [=]() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, func3());
    ABSL_ASSIGN_OR_RETURN(ptr, func2());
    return ptr;
  };
  int func4_line = __builtin_LINE() - 4;

  absl::StatusOr<std::unique_ptr<int>> result = func4();
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
  CheckSourceLocation(result.status(), {func2_line, func3_line, func4_line});
}

TEST(AssignOrReturn, NotChainSourceLocationWithEmptyMsg) {
  auto func1 = []() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(1));
    return ptr;
  };
  auto func2 = []() -> absl::StatusOr<std::unique_ptr<int>> {
    return absl::Status(absl::StatusCode::kInternal, "");
  };

  auto func3 = [=]() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, func1());
    ABSL_ASSIGN_OR_RETURN(ptr, func2());
    return ptr;
  };

  auto func4 = [=]() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, func3());
    ABSL_ASSIGN_OR_RETURN(ptr, func2());
    return ptr;
  };

  absl::StatusOr<std::unique_ptr<int>> result = func4();
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
  CheckSourceLocation(result.status());
}

TEST(AssignOrReturn, ChainSourceLocationWith3ArgStatusMacro) {
  auto func1 = []() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(1));
    return ptr;
  };
  auto func2 = []() -> absl::StatusOr<std::unique_ptr<int>> {
    return absl::Status(absl::StatusCode::kInternal, "");
  };

  auto func3 = [=]() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, func1());
    ABSL_ASSIGN_OR_RETURN(ptr, func2(), _ << "hmm");
    return ptr;
  };
  int func3_line = __builtin_LINE() - 3;

  auto func4 = [=]() -> absl::StatusOr<std::unique_ptr<int>> {
    std::unique_ptr<int> ptr;
    ABSL_ASSIGN_OR_RETURN(ptr, func3());
    ABSL_ASSIGN_OR_RETURN(ptr, func2());
    return ptr;
  };
  int func4_line = __builtin_LINE() - 4;

  absl::StatusOr<std::unique_ptr<int>> result = func4();
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
  CheckSourceLocation(result.status(), {func3_line, func4_line});
}

TEST(ReturnIfError, Works) {
  auto func = []() -> absl::Status {
    ABSL_RETURN_IF_ERROR(ReturnOk());
    ABSL_RETURN_IF_ERROR(ReturnOk());
    ABSL_RETURN_IF_ERROR(ReturnError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(ReturnIfError, WorksWithSourceLocation) {
  using StatusCode = absl::StatusCode;
  {
    auto func = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(StatusCode::kUnknown, "EXPECTED"));
      return ReturnError("ERROR");
    };
    int error_line = __builtin_LINE() - 3;
    int func_line = error_line;

    EXPECT_THAT(func().message(), Eq("EXPECTED"));
    CheckSourceLocation(func(), {error_line, func_line});
  }
  {
    auto func1 = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(StatusCode::kUnknown, "EXPECTED"));
      return ReturnError("ERROR");
    };
    int error_line = __builtin_LINE() - 3;
    int func_line = error_line;

    auto func3 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func1());
      return ReturnError("ERROR_Func3");
    };
    int func3_line = __builtin_LINE() - 3;

    auto func4 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func3());
      return ReturnError("ERROR_Func4");
    };
    int func4_line = __builtin_LINE() - 3;
    CheckSourceLocation(func4(),
                        {error_line, func_line, func3_line, func4_line});
  }
  {
    auto func1 = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(StatusCode::kUnknown, ""));
      return ReturnError("ERROR");
    };

    auto func3 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func1());
      return ReturnError("ERROR_Func3");
    };

    auto func4 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func3());
      return ReturnError("ERROR_Func4");
    };
    CheckSourceLocation(func4());
  }
}

TEST(ReturnIfError, WorksWithSourceLocationOn2Arg) {
  {
    auto func = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(absl::StatusCode::kUnknown, "EXPECTED"))
          << "foo";
      return ReturnError("ERROR");
    };
    int error_line = __builtin_LINE() - 4;
    int func_line = error_line;

    EXPECT_THAT(func().message(), Eq("EXPECTED; foo"));
    CheckSourceLocation(func(), {error_line, func_line});
  }
  {
    auto func1 = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(absl::StatusCode::kUnknown, "EXPECTED"))
          << "foo";
      return ReturnError("ERROR");
    };
    int error_line = __builtin_LINE() - 4;
    int func_line = error_line;

    auto func3 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func1());
      return ReturnError("ERROR_Func3");
    };
    int func3_line = __builtin_LINE() - 3;

    auto func4 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func3());
      return ReturnError("ERROR_Func4");
    };
    int func4_line = __builtin_LINE() - 3;
    CheckSourceLocation(func4(),
                        {error_line, func_line, func3_line, func4_line});
  }
  {
    auto func1 = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(absl::StatusCode::kUnknown, ""));
      return ReturnError("ERROR");
    };

    auto func3 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func1()) << "foo";
      return ReturnError("ERROR_Func3");
    };
    int func3_line = __builtin_LINE() - 3;

    auto func4 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func3());
      return ReturnError("ERROR_Func4");
    };
    int func4_line = __builtin_LINE() - 3;
    EXPECT_THAT(func4().message(), Eq("foo"));
    CheckSourceLocation(func4(), {func3_line, func4_line});
  }
  {
    auto func1 = []() -> absl::Status {
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(ReturnOk());
      ABSL_RETURN_IF_ERROR(absl::Status(absl::StatusCode::kUnknown, "")) << "";
      return ReturnError("ERROR");
    };

    auto func3 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func1());
      return ReturnError("ERROR_Func3");
    };

    auto func4 = [=]() -> absl::Status {
      ABSL_RETURN_IF_ERROR(func3());
      return ReturnError("ERROR_Func4");
    };
    CheckSourceLocation(func4());
  }
}

TEST(ReturnIfError, WorksWithBuilder) {
  auto func = []() -> absl::Status {
    ABSL_RETURN_IF_ERROR(ReturnOkBuilder());
    ABSL_RETURN_IF_ERROR(ReturnOkBuilder());
    ABSL_RETURN_IF_ERROR(ReturnErrorBuilder("EXPECTED"));
    return ReturnErrorBuilder("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));

  // Test that ABSL_RETURN_IF_ERROR can also return the StatusBuilder directly.
  auto func2 = []() -> absl::StatusBuilder {
    ABSL_RETURN_IF_ERROR(ReturnOkBuilder());
    ABSL_RETURN_IF_ERROR(ReturnOkBuilder());
    ABSL_RETURN_IF_ERROR(ReturnErrorBuilder("EXPECTED"));
    return ReturnErrorBuilder("ERROR");
  };
  EXPECT_THAT(static_cast<absl::Status>(func2()).message(), Eq("EXPECTED"));
}

TEST(ReturnIfError, IfInputIsBuilderDoesNotEagerlyConvertToStatus) {
  auto func = []() -> absl::Status {
    auto builder = ReturnErrorBuilder("FIRST");
    builder.SetPrepend();
    // If this call decays the builder to Status first and then makes another
    // builder it will forget about the SetPrepend and the streaming will happen
    // in the wrong order.
    ABSL_RETURN_IF_ERROR(builder) << "SECOND ";
    return absl::OkStatus();
  };
  EXPECT_THAT(func().message(), Eq("SECOND FIRST"));
}

TEST(ReturnIfError, WorksWithLambda) {
  auto func = []() -> absl::Status {
    ABSL_RETURN_IF_ERROR([] { return ReturnOk(); }());
    ABSL_RETURN_IF_ERROR([] { return ReturnError("EXPECTED"); }());
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(ReturnIfError, WorksWithAppend) {
  auto fail_test_if_called = []() -> std::string {
    ADD_FAILURE();
    return "FAILURE";
  };
  auto func = [&]() -> absl::Status {
    ABSL_RETURN_IF_ERROR(ReturnOk()) << fail_test_if_called();
    ABSL_RETURN_IF_ERROR(ReturnError("EXPECTED A")) << "EXPECTED B";
    return absl::OkStatus();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(ReturnIfError, WorksWithVoidReturnAdaptor) {
  int code = 0;
  int phase = 0;
  auto adaptor = [&](absl::Status status) -> void { code = phase; };
  auto func = [&]() -> void {
    phase = 1;
    ABSL_RETURN_IF_ERROR(ReturnOk()).With(adaptor);
    phase = 2;
    ABSL_RETURN_IF_ERROR(ReturnError("EXPECTED A")).With(adaptor);
    phase = 3;
  };

  func();
  EXPECT_EQ(phase, 2);
  EXPECT_EQ(code, 2);
}

}  // namespace
