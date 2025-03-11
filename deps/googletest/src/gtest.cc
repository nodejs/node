// Copyright 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

//
// The Google C++ Testing and Mocking Framework (Google Test)

#include "gtest/gtest.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <chrono>  // NOLINT
#include <cmath>
#include <csignal>  // NOLINT: raise(3) is used on some platforms
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <ostream>  // NOLINT
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gtest/gtest-assertion-result.h"
#include "gtest/gtest-spi.h"
#include "gtest/internal/custom/gtest.h"
#include "gtest/internal/gtest-port.h"

#ifdef GTEST_OS_LINUX

#include <fcntl.h>   // NOLINT
#include <limits.h>  // NOLINT
#include <sched.h>   // NOLINT
// Declares vsnprintf().  This header is not available on Windows.
#include <strings.h>   // NOLINT
#include <sys/mman.h>  // NOLINT
#include <sys/time.h>  // NOLINT
#include <unistd.h>    // NOLINT

#include <string>

#elif defined(GTEST_OS_ZOS)
#include <sys/time.h>  // NOLINT

// On z/OS we additionally need strings.h for strcasecmp.
#include <strings.h>   // NOLINT

#elif defined(GTEST_OS_WINDOWS_MOBILE)  // We are on Windows CE.

#include <windows.h>  // NOLINT
#undef min

#elif defined(GTEST_OS_WINDOWS)  // We are on Windows proper.

#include <windows.h>  // NOLINT
#undef min

#ifdef _MSC_VER
#include <crtdbg.h>  // NOLINT
#endif

#include <io.h>         // NOLINT
#include <sys/stat.h>   // NOLINT
#include <sys/timeb.h>  // NOLINT
#include <sys/types.h>  // NOLINT

#ifdef GTEST_OS_WINDOWS_MINGW
#include <sys/time.h>  // NOLINT
#endif                 // GTEST_OS_WINDOWS_MINGW

#else

// cpplint thinks that the header is already included, so we want to
// silence it.
#include <sys/time.h>  // NOLINT
#include <unistd.h>    // NOLINT

#endif  // GTEST_OS_LINUX

#if GTEST_HAS_EXCEPTIONS
#include <stdexcept>
#endif

#if GTEST_CAN_STREAM_RESULTS_
#include <arpa/inet.h>   // NOLINT
#include <netdb.h>       // NOLINT
#include <sys/socket.h>  // NOLINT
#include <sys/types.h>   // NOLINT
#endif

#include "src/gtest-internal-inl.h"

#ifdef GTEST_OS_WINDOWS
#define vsnprintf _vsnprintf
#endif  // GTEST_OS_WINDOWS

#ifdef GTEST_OS_MAC
#ifndef GTEST_OS_IOS
#include <crt_externs.h>
#endif
#endif

#ifdef GTEST_HAS_ABSL
#include "absl/container/flat_hash_set.h"
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#endif  // GTEST_HAS_ABSL

// Checks builtin compiler feature |x| while avoiding an extra layer of #ifdefs
// at the callsite.
#if defined(__has_builtin)
#define GTEST_HAS_BUILTIN(x) __has_builtin(x)
#else
#define GTEST_HAS_BUILTIN(x) 0
#endif  // defined(__has_builtin)

#if defined(GTEST_HAS_ABSL) && !defined(GTEST_NO_ABSL_FLAGS)
#define GTEST_HAS_ABSL_FLAGS
#endif

namespace testing {

using internal::CountIf;
using internal::ForEach;
using internal::GetElementOr;
using internal::Shuffle;

// Constants.

// A test whose test suite name or test name matches this filter is
// disabled and not run.
static const char kDisableTestFilter[] = "DISABLED_*:*/DISABLED_*";

// A test suite whose name matches this filter is considered a death
// test suite and will be run before test suites whose name doesn't
// match this filter.
static const char kDeathTestSuiteFilter[] = "*DeathTest:*DeathTest/*";

// A test filter that matches everything.
static const char kUniversalFilter[] = "*";

// The default output format.
static const char kDefaultOutputFormat[] = "xml";
// The default output file.
static const char kDefaultOutputFile[] = "test_detail";

// The environment variable name for the test shard index.
static const char kTestShardIndex[] = "GTEST_SHARD_INDEX";
// The environment variable name for the total number of test shards.
static const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard status file.
static const char kTestShardStatusFile[] = "GTEST_SHARD_STATUS_FILE";

namespace internal {

// The text used in failure messages to indicate the start of the
// stack trace.
const char kStackTraceMarker[] = "\nStack trace:\n";

// g_help_flag is true if and only if the --help flag or an equivalent form
// is specified on the command line.
bool g_help_flag = false;

#if GTEST_HAS_FILE_SYSTEM
// Utility function to Open File for Writing
static FILE* OpenFileForWriting(const std::string& output_file) {
  FILE* fileout = nullptr;
  FilePath output_file_path(output_file);
  FilePath output_dir(output_file_path.RemoveFileName());

  if (output_dir.CreateDirectoriesRecursively()) {
    fileout = posix::FOpen(output_file.c_str(), "w");
  }
  if (fileout == nullptr) {
    GTEST_LOG_(FATAL) << "Unable to open file \"" << output_file << "\"";
  }
  return fileout;
}
#endif  // GTEST_HAS_FILE_SYSTEM

}  // namespace internal

// Bazel passes in the argument to '--test_filter' via the TESTBRIDGE_TEST_ONLY
// environment variable.
static const char* GetDefaultFilter() {
  const char* const testbridge_test_only =
      internal::posix::GetEnv("TESTBRIDGE_TEST_ONLY");
  if (testbridge_test_only != nullptr) {
    return testbridge_test_only;
  }
  return kUniversalFilter;
}

// Bazel passes in the argument to '--test_runner_fail_fast' via the
// TESTBRIDGE_TEST_RUNNER_FAIL_FAST environment variable.
static bool GetDefaultFailFast() {
  const char* const testbridge_test_runner_fail_fast =
      internal::posix::GetEnv("TESTBRIDGE_TEST_RUNNER_FAIL_FAST");
  if (testbridge_test_runner_fail_fast != nullptr) {
    return strcmp(testbridge_test_runner_fail_fast, "1") == 0;
  }
  return false;
}

}  // namespace testing

GTEST_DEFINE_bool_(
    fail_fast,
    testing::internal::BoolFromGTestEnv("fail_fast",
                                        testing::GetDefaultFailFast()),
    "True if and only if a test failure should stop further test execution.");

GTEST_DEFINE_bool_(
    also_run_disabled_tests,
    testing::internal::BoolFromGTestEnv("also_run_disabled_tests", false),
    "Run disabled tests too, in addition to the tests normally being run.");

GTEST_DEFINE_bool_(
    break_on_failure,
    testing::internal::BoolFromGTestEnv("break_on_failure", false),
    "True if and only if a failed assertion should be a debugger "
    "break-point.");

GTEST_DEFINE_bool_(catch_exceptions,
                   testing::internal::BoolFromGTestEnv("catch_exceptions",
                                                       true),
                   "True if and only if " GTEST_NAME_
                   " should catch exceptions and treat them as test failures.");

GTEST_DEFINE_string_(
    color, testing::internal::StringFromGTestEnv("color", "auto"),
    "Whether to use colors in the output.  Valid values: yes, no, "
    "and auto.  'auto' means to use colors if the output is "
    "being sent to a terminal and the TERM environment variable "
    "is set to a terminal type that supports colors.");

GTEST_DEFINE_string_(
    filter,
    testing::internal::StringFromGTestEnv("filter",
                                          testing::GetDefaultFilter()),
    "A colon-separated list of glob (not regex) patterns "
    "for filtering the tests to run, optionally followed by a "
    "'-' and a : separated list of negative patterns (tests to "
    "exclude).  A test is run if it matches one of the positive "
    "patterns and does not match any of the negative patterns.");

GTEST_DEFINE_bool_(
    install_failure_signal_handler,
    testing::internal::BoolFromGTestEnv("install_failure_signal_handler",
                                        false),
    "If true and supported on the current platform, " GTEST_NAME_
    " should "
    "install a signal handler that dumps debugging information when fatal "
    "signals are raised.");

GTEST_DEFINE_bool_(list_tests, false, "List all tests without running them.");

// The net priority order after flag processing is thus:
//   --gtest_output command line flag
//   GTEST_OUTPUT environment variable
//   XML_OUTPUT_FILE environment variable
//   ''
GTEST_DEFINE_string_(
    output,
    testing::internal::StringFromGTestEnv(
        "output", testing::internal::OutputFlagAlsoCheckEnvVar().c_str()),
    "A format (defaults to \"xml\" but can be specified to be \"json\"), "
    "optionally followed by a colon and an output file name or directory. "
    "A directory is indicated by a trailing pathname separator. "
    "Examples: \"xml:filename.xml\", \"xml::directoryname/\". "
    "If a directory is specified, output files will be created "
    "within that directory, with file-names based on the test "
    "executable's name and, if necessary, made unique by adding "
    "digits.");

GTEST_DEFINE_bool_(
    brief, testing::internal::BoolFromGTestEnv("brief", false),
    "True if only test failures should be displayed in text output.");

GTEST_DEFINE_bool_(print_time,
                   testing::internal::BoolFromGTestEnv("print_time", true),
                   "True if and only if " GTEST_NAME_
                   " should display elapsed time in text output.");

GTEST_DEFINE_bool_(print_utf8,
                   testing::internal::BoolFromGTestEnv("print_utf8", true),
                   "True if and only if " GTEST_NAME_
                   " prints UTF8 characters as text.");

GTEST_DEFINE_int32_(
    random_seed, testing::internal::Int32FromGTestEnv("random_seed", 0),
    "Random number seed to use when shuffling test orders.  Must be in range "
    "[1, 99999], or 0 to use a seed based on the current time.");

GTEST_DEFINE_int32_(
    repeat, testing::internal::Int32FromGTestEnv("repeat", 1),
    "How many times to repeat each test.  Specify a negative number "
    "for repeating forever.  Useful for shaking out flaky tests.");

GTEST_DEFINE_bool_(
    recreate_environments_when_repeating,
    testing::internal::BoolFromGTestEnv("recreate_environments_when_repeating",
                                        false),
    "Controls whether global test environments are recreated for each repeat "
    "of the tests. If set to false the global test environments are only set "
    "up once, for the first iteration, and only torn down once, for the last. "
    "Useful for shaking out flaky tests with stable, expensive test "
    "environments. If --gtest_repeat is set to a negative number, meaning "
    "there is no last run, the environments will always be recreated to avoid "
    "leaks.");

GTEST_DEFINE_bool_(show_internal_stack_frames, false,
                   "True if and only if " GTEST_NAME_
                   " should include internal stack frames when "
                   "printing test failure stack traces.");

GTEST_DEFINE_bool_(shuffle,
                   testing::internal::BoolFromGTestEnv("shuffle", false),
                   "True if and only if " GTEST_NAME_
                   " should randomize tests' order on every run.");

GTEST_DEFINE_int32_(
    stack_trace_depth,
    testing::internal::Int32FromGTestEnv("stack_trace_depth",
                                         testing::kMaxStackTraceDepth),
    "The maximum number of stack frames to print when an "
    "assertion fails.  The valid range is 0 through 100, inclusive.");

GTEST_DEFINE_string_(
    stream_result_to,
    testing::internal::StringFromGTestEnv("stream_result_to", ""),
    "This flag specifies the host name and the port number on which to stream "
    "test results. Example: \"localhost:555\". The flag is effective only on "
    "Linux and macOS.");

GTEST_DEFINE_bool_(
    throw_on_failure,
    testing::internal::BoolFromGTestEnv("throw_on_failure", false),
    "When this flag is specified, a failed assertion will throw an exception "
    "if exceptions are enabled or exit the program with a non-zero code "
    "otherwise. For use with an external test framework.");

#if GTEST_USE_OWN_FLAGFILE_FLAG_
GTEST_DEFINE_string_(
    flagfile, testing::internal::StringFromGTestEnv("flagfile", ""),
    "This flag specifies the flagfile to read command-line flags from.");
#endif  // GTEST_USE_OWN_FLAGFILE_FLAG_

namespace testing {
namespace internal {

const uint32_t Random::kMaxRange;

// Generates a random number from [0, range), using a Linear
// Congruential Generator (LCG).  Crashes if 'range' is 0 or greater
// than kMaxRange.
uint32_t Random::Generate(uint32_t range) {
  // These constants are the same as are used in glibc's rand(3).
  // Use wider types than necessary to prevent unsigned overflow diagnostics.
  state_ = static_cast<uint32_t>(1103515245ULL * state_ + 12345U) % kMaxRange;

  GTEST_CHECK_(range > 0) << "Cannot generate a number in the range [0, 0).";
  GTEST_CHECK_(range <= kMaxRange)
      << "Generation of a number in [0, " << range << ") was requested, "
      << "but this can only generate numbers in [0, " << kMaxRange << ").";

  // Converting via modulus introduces a bit of downward bias, but
  // it's simple, and a linear congruential generator isn't too good
  // to begin with.
  return state_ % range;
}

// GTestIsInitialized() returns true if and only if the user has initialized
// Google Test.  Useful for catching the user mistake of not initializing
// Google Test before calling RUN_ALL_TESTS().
static bool GTestIsInitialized() { return !GetArgvs().empty(); }

// Iterates over a vector of TestSuites, keeping a running sum of the
// results of calling a given int-returning method on each.
// Returns the sum.
static int SumOverTestSuiteList(const std::vector<TestSuite*>& case_list,
                                int (TestSuite::*method)() const) {
  int sum = 0;
  for (size_t i = 0; i < case_list.size(); i++) {
    sum += (case_list[i]->*method)();
  }
  return sum;
}

// Returns true if and only if the test suite passed.
static bool TestSuitePassed(const TestSuite* test_suite) {
  return test_suite->should_run() && test_suite->Passed();
}

// Returns true if and only if the test suite failed.
static bool TestSuiteFailed(const TestSuite* test_suite) {
  return test_suite->should_run() && test_suite->Failed();
}

// Returns true if and only if test_suite contains at least one test that
// should run.
static bool ShouldRunTestSuite(const TestSuite* test_suite) {
  return test_suite->should_run();
}

namespace {

// Returns true if test part results of type `type` should include a stack
// trace.
bool ShouldEmitStackTraceForResultType(TestPartResult::Type type) {
  // Suppress emission of the stack trace for SUCCEED() since it likely never
  // requires investigation, and GTEST_SKIP() since skipping is an intentional
  // act by the developer rather than a failure requiring investigation.
  return type != TestPartResult::kSuccess && type != TestPartResult::kSkip;
}

}  // namespace

// AssertHelper constructor.
AssertHelper::AssertHelper(TestPartResult::Type type, const char* file,
                           int line, const char* message)
    : data_(new AssertHelperData(type, file, line, message)) {}

AssertHelper::~AssertHelper() { delete data_; }

// Message assignment, for assertion streaming support.
void AssertHelper::operator=(const Message& message) const {
  UnitTest::GetInstance()->AddTestPartResult(
      data_->type, data_->file, data_->line,
      AppendUserMessage(data_->message, message),
      ShouldEmitStackTraceForResultType(data_->type)
          ? UnitTest::GetInstance()->impl()->CurrentOsStackTraceExceptTop(1)
          : ""
      // Skips the stack frame for this function itself.
  );  // NOLINT
}

namespace {

// When TEST_P is found without a matching INSTANTIATE_TEST_SUITE_P
// to creates test cases for it, a synthetic test case is
// inserted to report ether an error or a log message.
//
// This configuration bit will likely be removed at some point.
constexpr bool kErrorOnUninstantiatedParameterizedTest = true;
constexpr bool kErrorOnUninstantiatedTypeParameterizedTest = true;

// A test that fails at a given file/line location with a given message.
class FailureTest : public Test {
 public:
  explicit FailureTest(const CodeLocation& loc, std::string error_message,
                       bool as_error)
      : loc_(loc),
        error_message_(std::move(error_message)),
        as_error_(as_error) {}

  void TestBody() override {
    if (as_error_) {
      AssertHelper(TestPartResult::kNonFatalFailure, loc_.file.c_str(),
                   loc_.line, "") = Message() << error_message_;
    } else {
      std::cout << error_message_ << std::endl;
    }
  }

 private:
  const CodeLocation loc_;
  const std::string error_message_;
  const bool as_error_;
};

}  // namespace

std::set<std::string>* GetIgnoredParameterizedTestSuites() {
  return UnitTest::GetInstance()->impl()->ignored_parameterized_test_suites();
}

// Add a given test_suit to the list of them allow to go un-instantiated.
MarkAsIgnored::MarkAsIgnored(const char* test_suite) {
  GetIgnoredParameterizedTestSuites()->insert(test_suite);
}

// If this parameterized test suite has no instantiations (and that
// has not been marked as okay), emit a test case reporting that.
void InsertSyntheticTestCase(const std::string& name, CodeLocation location,
                             bool has_test_p) {
  const auto& ignored = *GetIgnoredParameterizedTestSuites();
  if (ignored.find(name) != ignored.end()) return;

  const char kMissingInstantiation[] =  //
      " is defined via TEST_P, but never instantiated. None of the test "
      "cases "
      "will run. Either no INSTANTIATE_TEST_SUITE_P is provided or the only "
      "ones provided expand to nothing."
      "\n\n"
      "Ideally, TEST_P definitions should only ever be included as part of "
      "binaries that intend to use them. (As opposed to, for example, being "
      "placed in a library that may be linked in to get other utilities.)";

  const char kMissingTestCase[] =  //
      " is instantiated via INSTANTIATE_TEST_SUITE_P, but no tests are "
      "defined via TEST_P . No test cases will run."
      "\n\n"
      "Ideally, INSTANTIATE_TEST_SUITE_P should only ever be invoked from "
      "code that always depend on code that provides TEST_P. Failing to do "
      "so is often an indication of dead code, e.g. the last TEST_P was "
      "removed but the rest got left behind.";

  std::string message =
      "Parameterized test suite " + name +
      (has_test_p ? kMissingInstantiation : kMissingTestCase) +
      "\n\n"
      "To suppress this error for this test suite, insert the following line "
      "(in a non-header) in the namespace it is defined in:"
      "\n\n"
      "GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(" +
      name + ");";

  std::string full_name = "UninstantiatedParameterizedTestSuite<" + name + ">";
  RegisterTest(  //
      "GoogleTestVerification", full_name.c_str(),
      nullptr,  // No type parameter.
      nullptr,  // No value parameter.
      location.file.c_str(), location.line, [message, location] {
        return new FailureTest(location, message,
                               kErrorOnUninstantiatedParameterizedTest);
      });
}

void RegisterTypeParameterizedTestSuite(const char* test_suite_name,
                                        CodeLocation code_location) {
  GetUnitTestImpl()->type_parameterized_test_registry().RegisterTestSuite(
      test_suite_name, std::move(code_location));
}

void RegisterTypeParameterizedTestSuiteInstantiation(const char* case_name) {
  GetUnitTestImpl()->type_parameterized_test_registry().RegisterInstantiation(
      case_name);
}

void TypeParameterizedTestSuiteRegistry::RegisterTestSuite(
    const char* test_suite_name, CodeLocation code_location) {
  suites_.emplace(std::string(test_suite_name),
                  TypeParameterizedTestSuiteInfo(std::move(code_location)));
}

void TypeParameterizedTestSuiteRegistry::RegisterInstantiation(
    const char* test_suite_name) {
  auto it = suites_.find(std::string(test_suite_name));
  if (it != suites_.end()) {
    it->second.instantiated = true;
  } else {
    GTEST_LOG_(ERROR) << "Unknown type parameterized test suit '"
                      << test_suite_name << "'";
  }
}

void TypeParameterizedTestSuiteRegistry::CheckForInstantiations() {
  const auto& ignored = *GetIgnoredParameterizedTestSuites();
  for (const auto& testcase : suites_) {
    if (testcase.second.instantiated) continue;
    if (ignored.find(testcase.first) != ignored.end()) continue;

    std::string message =
        "Type parameterized test suite " + testcase.first +
        " is defined via REGISTER_TYPED_TEST_SUITE_P, but never instantiated "
        "via INSTANTIATE_TYPED_TEST_SUITE_P. None of the test cases will run."
        "\n\n"
        "Ideally, TYPED_TEST_P definitions should only ever be included as "
        "part of binaries that intend to use them. (As opposed to, for "
        "example, being placed in a library that may be linked in to get "
        "other "
        "utilities.)"
        "\n\n"
        "To suppress this error for this test suite, insert the following "
        "line "
        "(in a non-header) in the namespace it is defined in:"
        "\n\n"
        "GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(" +
        testcase.first + ");";

    std::string full_name =
        "UninstantiatedTypeParameterizedTestSuite<" + testcase.first + ">";
    RegisterTest(  //
        "GoogleTestVerification", full_name.c_str(),
        nullptr,  // No type parameter.
        nullptr,  // No value parameter.
        testcase.second.code_location.file.c_str(),
        testcase.second.code_location.line, [message, testcase] {
          return new FailureTest(testcase.second.code_location, message,
                                 kErrorOnUninstantiatedTypeParameterizedTest);
        });
  }
}

// A copy of all command line arguments.  Set by InitGoogleTest().
static ::std::vector<std::string> g_argvs;

::std::vector<std::string> GetArgvs() {
#if defined(GTEST_CUSTOM_GET_ARGVS_)
  // GTEST_CUSTOM_GET_ARGVS_() may return a container of std::string or
  // ::string. This code converts it to the appropriate type.
  const auto& custom = GTEST_CUSTOM_GET_ARGVS_();
  return ::std::vector<std::string>(custom.begin(), custom.end());
#else   // defined(GTEST_CUSTOM_GET_ARGVS_)
  return g_argvs;
#endif  // defined(GTEST_CUSTOM_GET_ARGVS_)
}

#if GTEST_HAS_FILE_SYSTEM
// Returns the current application's name, removing directory path if that
// is present.
FilePath GetCurrentExecutableName() {
  FilePath result;

  auto args = GetArgvs();
  if (!args.empty()) {
#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_OS2)
    result.Set(FilePath(args[0]).RemoveExtension("exe"));
#else
    result.Set(FilePath(args[0]));
#endif  // GTEST_OS_WINDOWS
  }

  return result.RemoveDirectoryName();
}
#endif  // GTEST_HAS_FILE_SYSTEM

// Functions for processing the gtest_output flag.

// Returns the output format, or "" for normal printed output.
std::string UnitTestOptions::GetOutputFormat() {
  std::string s = GTEST_FLAG_GET(output);
  const char* const gtest_output_flag = s.c_str();
  const char* const colon = strchr(gtest_output_flag, ':');
  return (colon == nullptr)
             ? std::string(gtest_output_flag)
             : std::string(gtest_output_flag,
                           static_cast<size_t>(colon - gtest_output_flag));
}

#if GTEST_HAS_FILE_SYSTEM
// Returns the name of the requested output file, or the default if none
// was explicitly specified.
std::string UnitTestOptions::GetAbsolutePathToOutputFile() {
  std::string s = GTEST_FLAG_GET(output);
  const char* const gtest_output_flag = s.c_str();

  std::string format = GetOutputFormat();
  if (format.empty()) format = std::string(kDefaultOutputFormat);

  const char* const colon = strchr(gtest_output_flag, ':');
  if (colon == nullptr)
    return internal::FilePath::MakeFileName(
               internal::FilePath(
                   UnitTest::GetInstance()->original_working_dir()),
               internal::FilePath(kDefaultOutputFile), 0, format.c_str())
        .string();

  internal::FilePath output_name(colon + 1);
  if (!output_name.IsAbsolutePath())
    output_name = internal::FilePath::ConcatPaths(
        internal::FilePath(UnitTest::GetInstance()->original_working_dir()),
        internal::FilePath(colon + 1));

  if (!output_name.IsDirectory()) return output_name.string();

  internal::FilePath result(internal::FilePath::GenerateUniqueFileName(
      output_name, internal::GetCurrentExecutableName(),
      GetOutputFormat().c_str()));
  return result.string();
}
#endif  // GTEST_HAS_FILE_SYSTEM

// Returns true if and only if the wildcard pattern matches the string. Each
// pattern consists of regular characters, single-character wildcards (?), and
// multi-character wildcards (*).
//
// This function implements a linear-time string globbing algorithm based on
// https://research.swtch.com/glob.
static bool PatternMatchesString(const std::string& name_str,
                                 const char* pattern, const char* pattern_end) {
  const char* name = name_str.c_str();
  const char* const name_begin = name;
  const char* const name_end = name + name_str.size();

  const char* pattern_next = pattern;
  const char* name_next = name;

  while (pattern < pattern_end || name < name_end) {
    if (pattern < pattern_end) {
      switch (*pattern) {
        default:  // Match an ordinary character.
          if (name < name_end && *name == *pattern) {
            ++pattern;
            ++name;
            continue;
          }
          break;
        case '?':  // Match any single character.
          if (name < name_end) {
            ++pattern;
            ++name;
            continue;
          }
          break;
        case '*':
          // Match zero or more characters. Start by skipping over the wildcard
          // and matching zero characters from name. If that fails, restart and
          // match one more character than the last attempt.
          pattern_next = pattern;
          name_next = name + 1;
          ++pattern;
          continue;
      }
    }
    // Failed to match a character. Restart if possible.
    if (name_begin < name_next && name_next <= name_end) {
      pattern = pattern_next;
      name = name_next;
      continue;
    }
    return false;
  }
  return true;
}

namespace {

bool IsGlobPattern(const std::string& pattern) {
  return std::any_of(pattern.begin(), pattern.end(),
                     [](const char c) { return c == '?' || c == '*'; });
}

class UnitTestFilter {
 public:
  UnitTestFilter() = default;

  // Constructs a filter from a string of patterns separated by `:`.
  explicit UnitTestFilter(const std::string& filter) {
    // By design "" filter matches "" string.
    std::vector<std::string> all_patterns;
    SplitString(filter, ':', &all_patterns);
    const auto exact_match_patterns_begin = std::partition(
        all_patterns.begin(), all_patterns.end(), &IsGlobPattern);

    glob_patterns_.reserve(static_cast<size_t>(
        std::distance(all_patterns.begin(), exact_match_patterns_begin)));
    std::move(all_patterns.begin(), exact_match_patterns_begin,
              std::inserter(glob_patterns_, glob_patterns_.begin()));
    std::move(
        exact_match_patterns_begin, all_patterns.end(),
        std::inserter(exact_match_patterns_, exact_match_patterns_.begin()));
  }

  // Returns true if and only if name matches at least one of the patterns in
  // the filter.
  bool MatchesName(const std::string& name) const {
    return exact_match_patterns_.find(name) != exact_match_patterns_.end() ||
           std::any_of(glob_patterns_.begin(), glob_patterns_.end(),
                       [&name](const std::string& pattern) {
                         return PatternMatchesString(
                             name, pattern.c_str(),
                             pattern.c_str() + pattern.size());
                       });
  }

 private:
  std::vector<std::string> glob_patterns_;
  std::unordered_set<std::string> exact_match_patterns_;
};

class PositiveAndNegativeUnitTestFilter {
 public:
  // Constructs a positive and a negative filter from a string. The string
  // contains a positive filter optionally followed by a '-' character and a
  // negative filter. In case only a negative filter is provided the positive
  // filter will be assumed "*".
  // A filter is a list of patterns separated by ':'.
  explicit PositiveAndNegativeUnitTestFilter(const std::string& filter) {
    std::vector<std::string> positive_and_negative_filters;

    // NOTE: `SplitString` always returns a non-empty container.
    SplitString(filter, '-', &positive_and_negative_filters);
    const auto& positive_filter = positive_and_negative_filters.front();

    if (positive_and_negative_filters.size() > 1) {
      positive_filter_ = UnitTestFilter(
          positive_filter.empty() ? kUniversalFilter : positive_filter);

      // TODO(b/214626361): Fail on multiple '-' characters
      // For the moment to preserve old behavior we concatenate the rest of the
      // string parts with `-` as separator to generate the negative filter.
      auto negative_filter_string = positive_and_negative_filters[1];
      for (std::size_t i = 2; i < positive_and_negative_filters.size(); i++)
        negative_filter_string =
            negative_filter_string + '-' + positive_and_negative_filters[i];
      negative_filter_ = UnitTestFilter(negative_filter_string);
    } else {
      // In case we don't have a negative filter and positive filter is ""
      // we do not use kUniversalFilter by design as opposed to when we have a
      // negative filter.
      positive_filter_ = UnitTestFilter(positive_filter);
    }
  }

  // Returns true if and only if test name (this is generated by appending test
  // suit name and test name via a '.' character) matches the positive filter
  // and does not match the negative filter.
  bool MatchesTest(const std::string& test_suite_name,
                   const std::string& test_name) const {
    return MatchesName(test_suite_name + "." + test_name);
  }

  // Returns true if and only if name matches the positive filter and does not
  // match the negative filter.
  bool MatchesName(const std::string& name) const {
    return positive_filter_.MatchesName(name) &&
           !negative_filter_.MatchesName(name);
  }

 private:
  UnitTestFilter positive_filter_;
  UnitTestFilter negative_filter_;
};
}  // namespace

bool UnitTestOptions::MatchesFilter(const std::string& name_str,
                                    const char* filter) {
  return UnitTestFilter(filter).MatchesName(name_str);
}

// Returns true if and only if the user-specified filter matches the test
// suite name and the test name.
bool UnitTestOptions::FilterMatchesTest(const std::string& test_suite_name,
                                        const std::string& test_name) {
  // Split --gtest_filter at '-', if there is one, to separate into
  // positive filter and negative filter portions
  return PositiveAndNegativeUnitTestFilter(GTEST_FLAG_GET(filter))
      .MatchesTest(test_suite_name, test_name);
}

#if GTEST_HAS_SEH
static std::string FormatSehExceptionMessage(DWORD exception_code,
                                             const char* location) {
  Message message;
  message << "SEH exception with code 0x" << std::setbase(16) << exception_code
          << std::setbase(10) << " thrown in " << location << ".";
  return message.GetString();
}

int UnitTestOptions::GTestProcessSEH(DWORD seh_code, const char* location) {
  // Google Test should handle a SEH exception if:
  //   1. the user wants it to, AND
  //   2. this is not a breakpoint exception or stack overflow, AND
  //   3. this is not a C++ exception (VC++ implements them via SEH,
  //      apparently).
  //
  // SEH exception code for C++ exceptions.
  // (see https://support.microsoft.com/kb/185294 for more information).
  const DWORD kCxxExceptionCode = 0xe06d7363;

  if (!GTEST_FLAG_GET(catch_exceptions) || seh_code == kCxxExceptionCode ||
      seh_code == EXCEPTION_BREAKPOINT ||
      seh_code == EXCEPTION_STACK_OVERFLOW) {
    return EXCEPTION_CONTINUE_SEARCH;  // Don't handle these exceptions
  }

  internal::ReportFailureInUnknownLocation(
      TestPartResult::kFatalFailure,
      FormatSehExceptionMessage(seh_code, location) +
          "\n"
          "Stack trace:\n" +
          ::testing::internal::GetCurrentOsStackTraceExceptTop(1));

  return EXCEPTION_EXECUTE_HANDLER;
}
#endif  // GTEST_HAS_SEH

}  // namespace internal

// The c'tor sets this object as the test part result reporter used by
// Google Test.  The 'result' parameter specifies where to report the
// results. Intercepts only failures from the current thread.
ScopedFakeTestPartResultReporter::ScopedFakeTestPartResultReporter(
    TestPartResultArray* result)
    : intercept_mode_(INTERCEPT_ONLY_CURRENT_THREAD), result_(result) {
  Init();
}

// The c'tor sets this object as the test part result reporter used by
// Google Test.  The 'result' parameter specifies where to report the
// results.
ScopedFakeTestPartResultReporter::ScopedFakeTestPartResultReporter(
    InterceptMode intercept_mode, TestPartResultArray* result)
    : intercept_mode_(intercept_mode), result_(result) {
  Init();
}

void ScopedFakeTestPartResultReporter::Init() {
  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  if (intercept_mode_ == INTERCEPT_ALL_THREADS) {
    old_reporter_ = impl->GetGlobalTestPartResultReporter();
    impl->SetGlobalTestPartResultReporter(this);
  } else {
    old_reporter_ = impl->GetTestPartResultReporterForCurrentThread();
    impl->SetTestPartResultReporterForCurrentThread(this);
  }
}

// The d'tor restores the test part result reporter used by Google Test
// before.
ScopedFakeTestPartResultReporter::~ScopedFakeTestPartResultReporter() {
  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  if (intercept_mode_ == INTERCEPT_ALL_THREADS) {
    impl->SetGlobalTestPartResultReporter(old_reporter_);
  } else {
    impl->SetTestPartResultReporterForCurrentThread(old_reporter_);
  }
}

// Increments the test part result count and remembers the result.
// This method is from the TestPartResultReporterInterface interface.
void ScopedFakeTestPartResultReporter::ReportTestPartResult(
    const TestPartResult& result) {
  result_->Append(result);
}

namespace internal {

// Returns the type ID of ::testing::Test.  We should always call this
// instead of GetTypeId< ::testing::Test>() to get the type ID of
// testing::Test.  This is to work around a suspected linker bug when
// using Google Test as a framework on Mac OS X.  The bug causes
// GetTypeId< ::testing::Test>() to return different values depending
// on whether the call is from the Google Test framework itself or
// from user test code.  GetTestTypeId() is guaranteed to always
// return the same value, as it always calls GetTypeId<>() from the
// gtest.cc, which is within the Google Test framework.
TypeId GetTestTypeId() { return GetTypeId<Test>(); }

// The value of GetTestTypeId() as seen from within the Google Test
// library.  This is solely for testing GetTestTypeId().
extern const TypeId kTestTypeIdInGoogleTest = GetTestTypeId();

// This predicate-formatter checks that 'results' contains a test part
// failure of the given type and that the failure message contains the
// given substring.
static AssertionResult HasOneFailure(const char* /* results_expr */,
                                     const char* /* type_expr */,
                                     const char* /* substr_expr */,
                                     const TestPartResultArray& results,
                                     TestPartResult::Type type,
                                     const std::string& substr) {
  const std::string expected(type == TestPartResult::kFatalFailure
                                 ? "1 fatal failure"
                                 : "1 non-fatal failure");
  Message msg;
  if (results.size() != 1) {
    msg << "Expected: " << expected << "\n"
        << "  Actual: " << results.size() << " failures";
    for (int i = 0; i < results.size(); i++) {
      msg << "\n" << results.GetTestPartResult(i);
    }
    return AssertionFailure() << msg;
  }

  const TestPartResult& r = results.GetTestPartResult(0);
  if (r.type() != type) {
    return AssertionFailure() << "Expected: " << expected << "\n"
                              << "  Actual:\n"
                              << r;
  }

  if (strstr(r.message(), substr.c_str()) == nullptr) {
    return AssertionFailure()
           << "Expected: " << expected << " containing \"" << substr << "\"\n"
           << "  Actual:\n"
           << r;
  }

  return AssertionSuccess();
}

// The constructor of SingleFailureChecker remembers where to look up
// test part results, what type of failure we expect, and what
// substring the failure message should contain.
SingleFailureChecker::SingleFailureChecker(const TestPartResultArray* results,
                                           TestPartResult::Type type,
                                           const std::string& substr)
    : results_(results), type_(type), substr_(substr) {}

// The destructor of SingleFailureChecker verifies that the given
// TestPartResultArray contains exactly one failure that has the given
// type and contains the given substring.  If that's not the case, a
// non-fatal failure will be generated.
SingleFailureChecker::~SingleFailureChecker() {
  EXPECT_PRED_FORMAT3(HasOneFailure, *results_, type_, substr_);
}

DefaultGlobalTestPartResultReporter::DefaultGlobalTestPartResultReporter(
    UnitTestImpl* unit_test)
    : unit_test_(unit_test) {}

void DefaultGlobalTestPartResultReporter::ReportTestPartResult(
    const TestPartResult& result) {
  unit_test_->current_test_result()->AddTestPartResult(result);
  unit_test_->listeners()->repeater()->OnTestPartResult(result);
}

DefaultPerThreadTestPartResultReporter::DefaultPerThreadTestPartResultReporter(
    UnitTestImpl* unit_test)
    : unit_test_(unit_test) {}

void DefaultPerThreadTestPartResultReporter::ReportTestPartResult(
    const TestPartResult& result) {
  unit_test_->GetGlobalTestPartResultReporter()->ReportTestPartResult(result);
}

// Returns the global test part result reporter.
TestPartResultReporterInterface*
UnitTestImpl::GetGlobalTestPartResultReporter() {
  internal::MutexLock lock(&global_test_part_result_reporter_mutex_);
  return global_test_part_result_reporter_;
}

// Sets the global test part result reporter.
void UnitTestImpl::SetGlobalTestPartResultReporter(
    TestPartResultReporterInterface* reporter) {
  internal::MutexLock lock(&global_test_part_result_reporter_mutex_);
  global_test_part_result_reporter_ = reporter;
}

// Returns the test part result reporter for the current thread.
TestPartResultReporterInterface*
UnitTestImpl::GetTestPartResultReporterForCurrentThread() {
  return per_thread_test_part_result_reporter_.get();
}

// Sets the test part result reporter for the current thread.
void UnitTestImpl::SetTestPartResultReporterForCurrentThread(
    TestPartResultReporterInterface* reporter) {
  per_thread_test_part_result_reporter_.set(reporter);
}

// Gets the number of successful test suites.
int UnitTestImpl::successful_test_suite_count() const {
  return CountIf(test_suites_, TestSuitePassed);
}

// Gets the number of failed test suites.
int UnitTestImpl::failed_test_suite_count() const {
  return CountIf(test_suites_, TestSuiteFailed);
}

// Gets the number of all test suites.
int UnitTestImpl::total_test_suite_count() const {
  return static_cast<int>(test_suites_.size());
}

// Gets the number of all test suites that contain at least one test
// that should run.
int UnitTestImpl::test_suite_to_run_count() const {
  return CountIf(test_suites_, ShouldRunTestSuite);
}

// Gets the number of successful tests.
int UnitTestImpl::successful_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::successful_test_count);
}

// Gets the number of skipped tests.
int UnitTestImpl::skipped_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::skipped_test_count);
}

// Gets the number of failed tests.
int UnitTestImpl::failed_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::failed_test_count);
}

// Gets the number of disabled tests that will be reported in the XML report.
int UnitTestImpl::reportable_disabled_test_count() const {
  return SumOverTestSuiteList(test_suites_,
                              &TestSuite::reportable_disabled_test_count);
}

// Gets the number of disabled tests.
int UnitTestImpl::disabled_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::disabled_test_count);
}

// Gets the number of tests to be printed in the XML report.
int UnitTestImpl::reportable_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::reportable_test_count);
}

// Gets the number of all tests.
int UnitTestImpl::total_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::total_test_count);
}

// Gets the number of tests that should run.
int UnitTestImpl::test_to_run_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::test_to_run_count);
}

// Returns the current OS stack trace as an std::string.
//
// The maximum number of stack frames to be included is specified by
// the gtest_stack_trace_depth flag.  The skip_count parameter
// specifies the number of top frames to be skipped, which doesn't
// count against the number of frames to be included.
//
// For example, if Foo() calls Bar(), which in turn calls
// CurrentOsStackTraceExceptTop(1), Foo() will be included in the
// trace but Bar() and CurrentOsStackTraceExceptTop() won't.
std::string UnitTestImpl::CurrentOsStackTraceExceptTop(int skip_count) {
  return os_stack_trace_getter()->CurrentStackTrace(
      static_cast<int>(GTEST_FLAG_GET(stack_trace_depth)), skip_count + 1
      // Skips the user-specified number of frames plus this function
      // itself.
  );  // NOLINT
}

// A helper class for measuring elapsed times.
class Timer {
 public:
  Timer() : start_(clock::now()) {}

  // Return time elapsed in milliseconds since the timer was created.
  TimeInMillis Elapsed() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
                                                                 start_)
        .count();
  }

 private:
  // Fall back to the system_clock when building with newlib on a system
  // without a monotonic clock.
#if defined(_NEWLIB_VERSION) && !defined(CLOCK_MONOTONIC)
  using clock = std::chrono::system_clock;
#else
  using clock = std::chrono::steady_clock;
#endif
  clock::time_point start_;
};

// Returns a timestamp as milliseconds since the epoch. Note this time may jump
// around subject to adjustments by the system, to measure elapsed time use
// Timer instead.
TimeInMillis GetTimeInMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now() -
             std::chrono::system_clock::from_time_t(0))
      .count();
}

// Utilities

// class String.

#ifdef GTEST_OS_WINDOWS_MOBILE
// Creates a UTF-16 wide string from the given ANSI string, allocating
// memory using new. The caller is responsible for deleting the return
// value using delete[]. Returns the wide string, or NULL if the
// input is NULL.
LPCWSTR String::AnsiToUtf16(const char* ansi) {
  if (!ansi) return nullptr;
  const int length = strlen(ansi);
  const int unicode_length =
      MultiByteToWideChar(CP_ACP, 0, ansi, length, nullptr, 0);
  WCHAR* unicode = new WCHAR[unicode_length + 1];
  MultiByteToWideChar(CP_ACP, 0, ansi, length, unicode, unicode_length);
  unicode[unicode_length] = 0;
  return unicode;
}

// Creates an ANSI string from the given wide string, allocating
// memory using new. The caller is responsible for deleting the return
// value using delete[]. Returns the ANSI string, or NULL if the
// input is NULL.
const char* String::Utf16ToAnsi(LPCWSTR utf16_str) {
  if (!utf16_str) return nullptr;
  const int ansi_length = WideCharToMultiByte(CP_ACP, 0, utf16_str, -1, nullptr,
                                              0, nullptr, nullptr);
  char* ansi = new char[ansi_length + 1];
  WideCharToMultiByte(CP_ACP, 0, utf16_str, -1, ansi, ansi_length, nullptr,
                      nullptr);
  ansi[ansi_length] = 0;
  return ansi;
}

#endif  // GTEST_OS_WINDOWS_MOBILE

// Compares two C strings.  Returns true if and only if they have the same
// content.
//
// Unlike strcmp(), this function can handle NULL argument(s).  A NULL
// C string is considered different to any non-NULL C string,
// including the empty string.
bool String::CStringEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr) return rhs == nullptr;

  if (rhs == nullptr) return false;

  return strcmp(lhs, rhs) == 0;
}

#if GTEST_HAS_STD_WSTRING

// Converts an array of wide chars to a narrow string using the UTF-8
// encoding, and streams the result to the given Message object.
static void StreamWideCharsToMessage(const wchar_t* wstr, size_t length,
                                     Message* msg) {
  for (size_t i = 0; i != length;) {  // NOLINT
    if (wstr[i] != L'\0') {
      *msg << WideStringToUtf8(wstr + i, static_cast<int>(length - i));
      while (i != length && wstr[i] != L'\0') i++;
    } else {
      *msg << '\0';
      i++;
    }
  }
}

#endif  // GTEST_HAS_STD_WSTRING

void SplitString(const ::std::string& str, char delimiter,
                 ::std::vector< ::std::string>* dest) {
  ::std::vector< ::std::string> parsed;
  ::std::string::size_type pos = 0;
  while (::testing::internal::AlwaysTrue()) {
    const ::std::string::size_type colon = str.find(delimiter, pos);
    if (colon == ::std::string::npos) {
      parsed.push_back(str.substr(pos));
      break;
    } else {
      parsed.push_back(str.substr(pos, colon - pos));
      pos = colon + 1;
    }
  }
  dest->swap(parsed);
}

}  // namespace internal

// Constructs an empty Message.
// We allocate the stringstream separately because otherwise each use of
// ASSERT/EXPECT in a procedure adds over 200 bytes to the procedure's
// stack frame leading to huge stack frames in some cases; gcc does not reuse
// the stack space.
Message::Message() : ss_(new ::std::stringstream) {
  // By default, we want there to be enough precision when printing
  // a double to a Message.
  *ss_ << std::setprecision(std::numeric_limits<double>::digits10 + 2);
}

// These two overloads allow streaming a wide C string to a Message
// using the UTF-8 encoding.
Message& Message::operator<<(const wchar_t* wide_c_str) {
  return *this << internal::String::ShowWideCString(wide_c_str);
}
Message& Message::operator<<(wchar_t* wide_c_str) {
  return *this << internal::String::ShowWideCString(wide_c_str);
}

#if GTEST_HAS_STD_WSTRING
// Converts the given wide string to a narrow string using the UTF-8
// encoding, and streams the result to this Message object.
Message& Message::operator<<(const ::std::wstring& wstr) {
  internal::StreamWideCharsToMessage(wstr.c_str(), wstr.length(), this);
  return *this;
}
#endif  // GTEST_HAS_STD_WSTRING

// Gets the text streamed to this object so far as an std::string.
// Each '\0' character in the buffer is replaced with "\\0".
std::string Message::GetString() const {
  return internal::StringStreamToString(ss_.get());
}

namespace internal {

namespace edit_distance {
std::vector<EditType> CalculateOptimalEdits(const std::vector<size_t>& left,
                                            const std::vector<size_t>& right) {
  std::vector<std::vector<double> > costs(
      left.size() + 1, std::vector<double>(right.size() + 1));
  std::vector<std::vector<EditType> > best_move(
      left.size() + 1, std::vector<EditType>(right.size() + 1));

  // Populate for empty right.
  for (size_t l_i = 0; l_i < costs.size(); ++l_i) {
    costs[l_i][0] = static_cast<double>(l_i);
    best_move[l_i][0] = kRemove;
  }
  // Populate for empty left.
  for (size_t r_i = 1; r_i < costs[0].size(); ++r_i) {
    costs[0][r_i] = static_cast<double>(r_i);
    best_move[0][r_i] = kAdd;
  }

  for (size_t l_i = 0; l_i < left.size(); ++l_i) {
    for (size_t r_i = 0; r_i < right.size(); ++r_i) {
      if (left[l_i] == right[r_i]) {
        // Found a match. Consume it.
        costs[l_i + 1][r_i + 1] = costs[l_i][r_i];
        best_move[l_i + 1][r_i + 1] = kMatch;
        continue;
      }

      const double add = costs[l_i + 1][r_i];
      const double remove = costs[l_i][r_i + 1];
      const double replace = costs[l_i][r_i];
      if (add < remove && add < replace) {
        costs[l_i + 1][r_i + 1] = add + 1;
        best_move[l_i + 1][r_i + 1] = kAdd;
      } else if (remove < add && remove < replace) {
        costs[l_i + 1][r_i + 1] = remove + 1;
        best_move[l_i + 1][r_i + 1] = kRemove;
      } else {
        // We make replace a little more expensive than add/remove to lower
        // their priority.
        costs[l_i + 1][r_i + 1] = replace + 1.00001;
        best_move[l_i + 1][r_i + 1] = kReplace;
      }
    }
  }

  // Reconstruct the best path. We do it in reverse order.
  std::vector<EditType> best_path;
  for (size_t l_i = left.size(), r_i = right.size(); l_i > 0 || r_i > 0;) {
    EditType move = best_move[l_i][r_i];
    best_path.push_back(move);
    l_i -= move != kAdd;
    r_i -= move != kRemove;
  }
  std::reverse(best_path.begin(), best_path.end());
  return best_path;
}

namespace {

// Helper class to convert string into ids with deduplication.
class InternalStrings {
 public:
  size_t GetId(const std::string& str) {
    IdMap::iterator it = ids_.find(str);
    if (it != ids_.end()) return it->second;
    size_t id = ids_.size();
    return ids_[str] = id;
  }

 private:
  typedef std::map<std::string, size_t> IdMap;
  IdMap ids_;
};

}  // namespace

std::vector<EditType> CalculateOptimalEdits(
    const std::vector<std::string>& left,
    const std::vector<std::string>& right) {
  std::vector<size_t> left_ids, right_ids;
  {
    InternalStrings intern_table;
    for (size_t i = 0; i < left.size(); ++i) {
      left_ids.push_back(intern_table.GetId(left[i]));
    }
    for (size_t i = 0; i < right.size(); ++i) {
      right_ids.push_back(intern_table.GetId(right[i]));
    }
  }
  return CalculateOptimalEdits(left_ids, right_ids);
}

namespace {

// Helper class that holds the state for one hunk and prints it out to the
// stream.
// It reorders adds/removes when possible to group all removes before all
// adds. It also adds the hunk header before printint into the stream.
class Hunk {
 public:
  Hunk(size_t left_start, size_t right_start)
      : left_start_(left_start),
        right_start_(right_start),
        adds_(),
        removes_(),
        common_() {}

  void PushLine(char edit, const char* line) {
    switch (edit) {
      case ' ':
        ++common_;
        FlushEdits();
        hunk_.push_back(std::make_pair(' ', line));
        break;
      case '-':
        ++removes_;
        hunk_removes_.push_back(std::make_pair('-', line));
        break;
      case '+':
        ++adds_;
        hunk_adds_.push_back(std::make_pair('+', line));
        break;
    }
  }

  void PrintTo(std::ostream* os) {
    PrintHeader(os);
    FlushEdits();
    for (std::list<std::pair<char, const char*> >::const_iterator it =
             hunk_.begin();
         it != hunk_.end(); ++it) {
      *os << it->first << it->second << "\n";
    }
  }

  bool has_edits() const { return adds_ || removes_; }

 private:
  void FlushEdits() {
    hunk_.splice(hunk_.end(), hunk_removes_);
    hunk_.splice(hunk_.end(), hunk_adds_);
  }

  // Print a unified diff header for one hunk.
  // The format is
  //   "@@ -<left_start>,<left_length> +<right_start>,<right_length> @@"
  // where the left/right parts are omitted if unnecessary.
  void PrintHeader(std::ostream* ss) const {
    *ss << "@@ ";
    if (removes_) {
      *ss << "-" << left_start_ << "," << (removes_ + common_);
    }
    if (removes_ && adds_) {
      *ss << " ";
    }
    if (adds_) {
      *ss << "+" << right_start_ << "," << (adds_ + common_);
    }
    *ss << " @@\n";
  }

  size_t left_start_, right_start_;
  size_t adds_, removes_, common_;
  std::list<std::pair<char, const char*> > hunk_, hunk_adds_, hunk_removes_;
};

}  // namespace

// Create a list of diff hunks in Unified diff format.
// Each hunk has a header generated by PrintHeader above plus a body with
// lines prefixed with ' ' for no change, '-' for deletion and '+' for
// addition.
// 'context' represents the desired unchanged prefix/suffix around the diff.
// If two hunks are close enough that their contexts overlap, then they are
// joined into one hunk.
std::string CreateUnifiedDiff(const std::vector<std::string>& left,
                              const std::vector<std::string>& right,
                              size_t context) {
  const std::vector<EditType> edits = CalculateOptimalEdits(left, right);

  size_t l_i = 0, r_i = 0, edit_i = 0;
  std::stringstream ss;
  while (edit_i < edits.size()) {
    // Find first edit.
    while (edit_i < edits.size() && edits[edit_i] == kMatch) {
      ++l_i;
      ++r_i;
      ++edit_i;
    }

    // Find the first line to include in the hunk.
    const size_t prefix_context = std::min(l_i, context);
    Hunk hunk(l_i - prefix_context + 1, r_i - prefix_context + 1);
    for (size_t i = prefix_context; i > 0; --i) {
      hunk.PushLine(' ', left[l_i - i].c_str());
    }

    // Iterate the edits until we found enough suffix for the hunk or the input
    // is over.
    size_t n_suffix = 0;
    for (; edit_i < edits.size(); ++edit_i) {
      if (n_suffix >= context) {
        // Continue only if the next hunk is very close.
        auto it = edits.begin() + static_cast<int>(edit_i);
        while (it != edits.end() && *it == kMatch) ++it;
        if (it == edits.end() ||
            static_cast<size_t>(it - edits.begin()) - edit_i >= context) {
          // There is no next edit or it is too far away.
          break;
        }
      }

      EditType edit = edits[edit_i];
      // Reset count when a non match is found.
      n_suffix = edit == kMatch ? n_suffix + 1 : 0;

      if (edit == kMatch || edit == kRemove || edit == kReplace) {
        hunk.PushLine(edit == kMatch ? ' ' : '-', left[l_i].c_str());
      }
      if (edit == kAdd || edit == kReplace) {
        hunk.PushLine('+', right[r_i].c_str());
      }

      // Advance indices, depending on edit type.
      l_i += edit != kAdd;
      r_i += edit != kRemove;
    }

    if (!hunk.has_edits()) {
      // We are done. We don't want this hunk.
      break;
    }

    hunk.PrintTo(&ss);
  }
  return ss.str();
}

}  // namespace edit_distance

namespace {

// The string representation of the values received in EqFailure() are already
// escaped. Split them on escaped '\n' boundaries. Leave all other escaped
// characters the same.
std::vector<std::string> SplitEscapedString(const std::string& str) {
  std::vector<std::string> lines;
  size_t start = 0, end = str.size();
  if (end > 2 && str[0] == '"' && str[end - 1] == '"') {
    ++start;
    --end;
  }
  bool escaped = false;
  for (size_t i = start; i + 1 < end; ++i) {
    if (escaped) {
      escaped = false;
      if (str[i] == 'n') {
        lines.push_back(str.substr(start, i - start - 1));
        start = i + 1;
      }
    } else {
      escaped = str[i] == '\\';
    }
  }
  lines.push_back(str.substr(start, end - start));
  return lines;
}

}  // namespace

// Constructs and returns the message for an equality assertion
// (e.g. ASSERT_EQ, EXPECT_STREQ, etc) failure.
//
// The first four parameters are the expressions used in the assertion
// and their values, as strings.  For example, for ASSERT_EQ(foo, bar)
// where foo is 5 and bar is 6, we have:
//
//   lhs_expression: "foo"
//   rhs_expression: "bar"
//   lhs_value:      "5"
//   rhs_value:      "6"
//
// The ignoring_case parameter is true if and only if the assertion is a
// *_STRCASEEQ*.  When it's true, the string "Ignoring case" will
// be inserted into the message.
AssertionResult EqFailure(const char* lhs_expression,
                          const char* rhs_expression,
                          const std::string& lhs_value,
                          const std::string& rhs_value, bool ignoring_case) {
  Message msg;
  msg << "Expected equality of these values:";
  msg << "\n  " << lhs_expression;
  if (lhs_value != lhs_expression) {
    msg << "\n    Which is: " << lhs_value;
  }
  msg << "\n  " << rhs_expression;
  if (rhs_value != rhs_expression) {
    msg << "\n    Which is: " << rhs_value;
  }

  if (ignoring_case) {
    msg << "\nIgnoring case";
  }

  if (!lhs_value.empty() && !rhs_value.empty()) {
    const std::vector<std::string> lhs_lines = SplitEscapedString(lhs_value);
    const std::vector<std::string> rhs_lines = SplitEscapedString(rhs_value);
    if (lhs_lines.size() > 1 || rhs_lines.size() > 1) {
      msg << "\nWith diff:\n"
          << edit_distance::CreateUnifiedDiff(lhs_lines, rhs_lines);
    }
  }

  return AssertionFailure() << msg;
}

// Constructs a failure message for Boolean assertions such as EXPECT_TRUE.
std::string GetBoolAssertionFailureMessage(
    const AssertionResult& assertion_result, const char* expression_text,
    const char* actual_predicate_value, const char* expected_predicate_value) {
  const char* actual_message = assertion_result.message();
  Message msg;
  msg << "Value of: " << expression_text
      << "\n  Actual: " << actual_predicate_value;
  if (actual_message[0] != '\0') msg << " (" << actual_message << ")";
  msg << "\nExpected: " << expected_predicate_value;
  return msg.GetString();
}

// Helper function for implementing ASSERT_NEAR. Treats infinity as a specific
// value, such that comparing infinity to infinity is equal, the distance
// between -infinity and +infinity is infinity, and infinity <= infinity is
// true.
AssertionResult DoubleNearPredFormat(const char* expr1, const char* expr2,
                                     const char* abs_error_expr, double val1,
                                     double val2, double abs_error) {
  // We want to return success when the two values are infinity and at least
  // one of the following is true:
  //  * The values are the same-signed infinity.
  //  * The error limit itself is infinity.
  // This is done here so that we don't end up with a NaN when calculating the
  // difference in values.
  if (std::isinf(val1) && std::isinf(val2) &&
      (std::signbit(val1) == std::signbit(val2) ||
       (abs_error > 0.0 && std::isinf(abs_error)))) {
    return AssertionSuccess();
  }

  const double diff = fabs(val1 - val2);
  if (diff <= abs_error) return AssertionSuccess();

  // Find the value which is closest to zero.
  const double min_abs = std::min(fabs(val1), fabs(val2));
  // Find the distance to the next double from that value.
  const double epsilon =
      nextafter(min_abs, std::numeric_limits<double>::infinity()) - min_abs;
  // Detect the case where abs_error is so small that EXPECT_NEAR is
  // effectively the same as EXPECT_EQUAL, and give an informative error
  // message so that the situation can be more easily understood without
  // requiring exotic floating-point knowledge.
  // Don't do an epsilon check if abs_error is zero because that implies
  // that an equality check was actually intended.
  if (!(std::isnan)(val1) && !(std::isnan)(val2) && abs_error > 0 &&
      abs_error < epsilon) {
    return AssertionFailure()
           << "The difference between " << expr1 << " and " << expr2 << " is "
           << diff << ", where\n"
           << expr1 << " evaluates to " << val1 << ",\n"
           << expr2 << " evaluates to " << val2 << ".\nThe abs_error parameter "
           << abs_error_expr << " evaluates to " << abs_error
           << " which is smaller than the minimum distance between doubles for "
              "numbers of this magnitude which is "
           << epsilon
           << ", thus making this EXPECT_NEAR check equivalent to "
              "EXPECT_EQUAL. Consider using EXPECT_DOUBLE_EQ instead.";
  }
  return AssertionFailure()
         << "The difference between " << expr1 << " and " << expr2 << " is "
         << diff << ", which exceeds " << abs_error_expr << ", where\n"
         << expr1 << " evaluates to " << val1 << ",\n"
         << expr2 << " evaluates to " << val2 << ", and\n"
         << abs_error_expr << " evaluates to " << abs_error << ".";
}

// Helper template for implementing FloatLE() and DoubleLE().
template <typename RawType>
AssertionResult FloatingPointLE(const char* expr1, const char* expr2,
                                RawType val1, RawType val2) {
  // Returns success if val1 is less than val2,
  if (val1 < val2) {
    return AssertionSuccess();
  }

  // or if val1 is almost equal to val2.
  const FloatingPoint<RawType> lhs(val1), rhs(val2);
  if (lhs.AlmostEquals(rhs)) {
    return AssertionSuccess();
  }

  // Note that the above two checks will both fail if either val1 or
  // val2 is NaN, as the IEEE floating-point standard requires that
  // any predicate involving a NaN must return false.

  ::std::stringstream val1_ss;
  val1_ss << std::setprecision(std::numeric_limits<RawType>::digits10 + 2)
          << val1;

  ::std::stringstream val2_ss;
  val2_ss << std::setprecision(std::numeric_limits<RawType>::digits10 + 2)
          << val2;

  return AssertionFailure()
         << "Expected: (" << expr1 << ") <= (" << expr2 << ")\n"
         << "  Actual: " << StringStreamToString(&val1_ss) << " vs "
         << StringStreamToString(&val2_ss);
}

}  // namespace internal

// Asserts that val1 is less than, or almost equal to, val2.  Fails
// otherwise.  In particular, it fails if either val1 or val2 is NaN.
AssertionResult FloatLE(const char* expr1, const char* expr2, float val1,
                        float val2) {
  return internal::FloatingPointLE<float>(expr1, expr2, val1, val2);
}

// Asserts that val1 is less than, or almost equal to, val2.  Fails
// otherwise.  In particular, it fails if either val1 or val2 is NaN.
AssertionResult DoubleLE(const char* expr1, const char* expr2, double val1,
                         double val2) {
  return internal::FloatingPointLE<double>(expr1, expr2, val1, val2);
}

namespace internal {

// The helper function for {ASSERT|EXPECT}_STREQ.
AssertionResult CmpHelperSTREQ(const char* lhs_expression,
                               const char* rhs_expression, const char* lhs,
                               const char* rhs) {
  if (String::CStringEquals(lhs, rhs)) {
    return AssertionSuccess();
  }

  return EqFailure(lhs_expression, rhs_expression, PrintToString(lhs),
                   PrintToString(rhs), false);
}

// The helper function for {ASSERT|EXPECT}_STRCASEEQ.
AssertionResult CmpHelperSTRCASEEQ(const char* lhs_expression,
                                   const char* rhs_expression, const char* lhs,
                                   const char* rhs) {
  if (String::CaseInsensitiveCStringEquals(lhs, rhs)) {
    return AssertionSuccess();
  }

  return EqFailure(lhs_expression, rhs_expression, PrintToString(lhs),
                   PrintToString(rhs), true);
}

// The helper function for {ASSERT|EXPECT}_STRNE.
AssertionResult CmpHelperSTRNE(const char* s1_expression,
                               const char* s2_expression, const char* s1,
                               const char* s2) {
  if (!String::CStringEquals(s1, s2)) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
           << "Expected: (" << s1_expression << ") != (" << s2_expression
           << "), actual: \"" << s1 << "\" vs \"" << s2 << "\"";
  }
}

// The helper function for {ASSERT|EXPECT}_STRCASENE.
AssertionResult CmpHelperSTRCASENE(const char* s1_expression,
                                   const char* s2_expression, const char* s1,
                                   const char* s2) {
  if (!String::CaseInsensitiveCStringEquals(s1, s2)) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
           << "Expected: (" << s1_expression << ") != (" << s2_expression
           << ") (ignoring case), actual: \"" << s1 << "\" vs \"" << s2 << "\"";
  }
}

}  // namespace internal

namespace {

// Helper functions for implementing IsSubString() and IsNotSubstring().

// This group of overloaded functions return true if and only if needle
// is a substring of haystack.  NULL is considered a substring of
// itself only.

bool IsSubstringPred(const char* needle, const char* haystack) {
  if (needle == nullptr || haystack == nullptr) return needle == haystack;

  return strstr(haystack, needle) != nullptr;
}

bool IsSubstringPred(const wchar_t* needle, const wchar_t* haystack) {
  if (needle == nullptr || haystack == nullptr) return needle == haystack;

  return wcsstr(haystack, needle) != nullptr;
}

// StringType here can be either ::std::string or ::std::wstring.
template <typename StringType>
bool IsSubstringPred(const StringType& needle, const StringType& haystack) {
  return haystack.find(needle) != StringType::npos;
}

// This function implements either IsSubstring() or IsNotSubstring(),
// depending on the value of the expected_to_be_substring parameter.
// StringType here can be const char*, const wchar_t*, ::std::string,
// or ::std::wstring.
template <typename StringType>
AssertionResult IsSubstringImpl(bool expected_to_be_substring,
                                const char* needle_expr,
                                const char* haystack_expr,
                                const StringType& needle,
                                const StringType& haystack) {
  if (IsSubstringPred(needle, haystack) == expected_to_be_substring)
    return AssertionSuccess();

  const bool is_wide_string = sizeof(needle[0]) > 1;
  const char* const begin_string_quote = is_wide_string ? "L\"" : "\"";
  return AssertionFailure()
         << "Value of: " << needle_expr << "\n"
         << "  Actual: " << begin_string_quote << needle << "\"\n"
         << "Expected: " << (expected_to_be_substring ? "" : "not ")
         << "a substring of " << haystack_expr << "\n"
         << "Which is: " << begin_string_quote << haystack << "\"";
}

}  // namespace

// IsSubstring() and IsNotSubstring() check whether needle is a
// substring of haystack (NULL is considered a substring of itself
// only), and return an appropriate error message when they fail.

AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const char* needle, const char* haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const wchar_t* needle, const wchar_t* haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr, const char* needle,
                               const char* haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr, const wchar_t* needle,
                               const wchar_t* haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const ::std::string& needle,
                            const ::std::string& haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr,
                               const ::std::string& needle,
                               const ::std::string& haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}

#if GTEST_HAS_STD_WSTRING
AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const ::std::wstring& needle,
                            const ::std::wstring& haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr,
                               const ::std::wstring& needle,
                               const ::std::wstring& haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}
#endif  // GTEST_HAS_STD_WSTRING

namespace internal {

#ifdef GTEST_OS_WINDOWS

namespace {

// Helper function for IsHRESULT{SuccessFailure} predicates
AssertionResult HRESULTFailureHelper(const char* expr, const char* expected,
                                     long hr) {  // NOLINT
#if defined(GTEST_OS_WINDOWS_MOBILE) || defined(GTEST_OS_WINDOWS_TV_TITLE)

  // Windows CE doesn't support FormatMessage.
  const char error_text[] = "";

#else

  // Looks up the human-readable system message for the HRESULT code
  // and since we're not passing any params to FormatMessage, we don't
  // want inserts expanded.
  const DWORD kFlags =
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD kBufSize = 4096;
  // Gets the system's human readable message string for this HRESULT.
  char error_text[kBufSize] = {'\0'};
  DWORD message_length = ::FormatMessageA(kFlags,
                                          0,  // no source, we're asking system
                                          static_cast<DWORD>(hr),  // the error
                                          0,  // no line width restrictions
                                          error_text,  // output buffer
                                          kBufSize,    // buf size
                                          nullptr);  // no arguments for inserts
  // Trims tailing white space (FormatMessage leaves a trailing CR-LF)
  for (; message_length && IsSpace(error_text[message_length - 1]);
       --message_length) {
    error_text[message_length - 1] = '\0';
  }

#endif  // GTEST_OS_WINDOWS_MOBILE

  const std::string error_hex("0x" + String::FormatHexInt(hr));
  return ::testing::AssertionFailure()
         << "Expected: " << expr << " " << expected << ".\n"
         << "  Actual: " << error_hex << " " << error_text << "\n";
}

}  // namespace

AssertionResult IsHRESULTSuccess(const char* expr, long hr) {  // NOLINT
  if (SUCCEEDED(hr)) {
    return AssertionSuccess();
  }
  return HRESULTFailureHelper(expr, "succeeds", hr);
}

AssertionResult IsHRESULTFailure(const char* expr, long hr) {  // NOLINT
  if (FAILED(hr)) {
    return AssertionSuccess();
  }
  return HRESULTFailureHelper(expr, "fails", hr);
}

#endif  // GTEST_OS_WINDOWS

// Utility functions for encoding Unicode text (wide strings) in
// UTF-8.

// A Unicode code-point can have up to 21 bits, and is encoded in UTF-8
// like this:
//
// Code-point length   Encoding
//   0 -  7 bits       0xxxxxxx
//   8 - 11 bits       110xxxxx 10xxxxxx
//  12 - 16 bits       1110xxxx 10xxxxxx 10xxxxxx
//  17 - 21 bits       11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

// The maximum code-point a one-byte UTF-8 sequence can represent.
constexpr uint32_t kMaxCodePoint1 = (static_cast<uint32_t>(1) << 7) - 1;

// The maximum code-point a two-byte UTF-8 sequence can represent.
constexpr uint32_t kMaxCodePoint2 = (static_cast<uint32_t>(1) << (5 + 6)) - 1;

// The maximum code-point a three-byte UTF-8 sequence can represent.
constexpr uint32_t kMaxCodePoint3 =
    (static_cast<uint32_t>(1) << (4 + 2 * 6)) - 1;

// The maximum code-point a four-byte UTF-8 sequence can represent.
constexpr uint32_t kMaxCodePoint4 =
    (static_cast<uint32_t>(1) << (3 + 3 * 6)) - 1;

// Chops off the n lowest bits from a bit pattern.  Returns the n
// lowest bits.  As a side effect, the original bit pattern will be
// shifted to the right by n bits.
inline uint32_t ChopLowBits(uint32_t* bits, int n) {
  const uint32_t low_bits = *bits & ((static_cast<uint32_t>(1) << n) - 1);
  *bits >>= n;
  return low_bits;
}

// Converts a Unicode code point to a narrow string in UTF-8 encoding.
// code_point parameter is of type uint32_t because wchar_t may not be
// wide enough to contain a code point.
// If the code_point is not a valid Unicode code point
// (i.e. outside of Unicode range U+0 to U+10FFFF) it will be converted
// to "(Invalid Unicode 0xXXXXXXXX)".
std::string CodePointToUtf8(uint32_t code_point) {
  if (code_point > kMaxCodePoint4) {
    return "(Invalid Unicode 0x" + String::FormatHexUInt32(code_point) + ")";
  }

  char str[5];  // Big enough for the largest valid code point.
  if (code_point <= kMaxCodePoint1) {
    str[1] = '\0';
    str[0] = static_cast<char>(code_point);  // 0xxxxxxx
  } else if (code_point <= kMaxCodePoint2) {
    str[2] = '\0';
    str[1] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));  // 10xxxxxx
    str[0] = static_cast<char>(0xC0 | code_point);                   // 110xxxxx
  } else if (code_point <= kMaxCodePoint3) {
    str[3] = '\0';
    str[2] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));  // 10xxxxxx
    str[1] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));  // 10xxxxxx
    str[0] = static_cast<char>(0xE0 | code_point);                   // 1110xxxx
  } else {  // code_point <= kMaxCodePoint4
    str[4] = '\0';
    str[3] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));  // 10xxxxxx
    str[2] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));  // 10xxxxxx
    str[1] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));  // 10xxxxxx
    str[0] = static_cast<char>(0xF0 | code_point);                   // 11110xxx
  }
  return str;
}

// The following two functions only make sense if the system
// uses UTF-16 for wide string encoding. All supported systems
// with 16 bit wchar_t (Windows, Cygwin) do use UTF-16.

// Determines if the arguments constitute UTF-16 surrogate pair
// and thus should be combined into a single Unicode code point
// using CreateCodePointFromUtf16SurrogatePair.
inline bool IsUtf16SurrogatePair(wchar_t first, wchar_t second) {
  return sizeof(wchar_t) == 2 && (first & 0xFC00) == 0xD800 &&
         (second & 0xFC00) == 0xDC00;
}

// Creates a Unicode code point from UTF16 surrogate pair.
inline uint32_t CreateCodePointFromUtf16SurrogatePair(wchar_t first,
                                                      wchar_t second) {
  const auto first_u = static_cast<uint32_t>(first);
  const auto second_u = static_cast<uint32_t>(second);
  const uint32_t mask = (1 << 10) - 1;
  return (sizeof(wchar_t) == 2)
             ? (((first_u & mask) << 10) | (second_u & mask)) + 0x10000
             :
             // This function should not be called when the condition is
             // false, but we provide a sensible default in case it is.
             first_u;
}

// Converts a wide string to a narrow string in UTF-8 encoding.
// The wide string is assumed to have the following encoding:
//   UTF-16 if sizeof(wchar_t) == 2 (on Windows, Cygwin)
//   UTF-32 if sizeof(wchar_t) == 4 (on Linux)
// Parameter str points to a null-terminated wide string.
// Parameter num_chars may additionally limit the number
// of wchar_t characters processed. -1 is used when the entire string
// should be processed.
// If the string contains code points that are not valid Unicode code points
// (i.e. outside of Unicode range U+0 to U+10FFFF) they will be output
// as '(Invalid Unicode 0xXXXXXXXX)'. If the string is in UTF16 encoding
// and contains invalid UTF-16 surrogate pairs, values in those pairs
// will be encoded as individual Unicode characters from Basic Normal Plane.
std::string WideStringToUtf8(const wchar_t* str, int num_chars) {
  if (num_chars == -1) num_chars = static_cast<int>(wcslen(str));

  ::std::stringstream stream;
  for (int i = 0; i < num_chars; ++i) {
    uint32_t unicode_code_point;

    if (str[i] == L'\0') {
      break;
    } else if (i + 1 < num_chars && IsUtf16SurrogatePair(str[i], str[i + 1])) {
      unicode_code_point =
          CreateCodePointFromUtf16SurrogatePair(str[i], str[i + 1]);
      i++;
    } else {
      unicode_code_point = static_cast<uint32_t>(str[i]);
    }

    stream << CodePointToUtf8(unicode_code_point);
  }
  return StringStreamToString(&stream);
}

// Converts a wide C string to an std::string using the UTF-8 encoding.
// NULL will be converted to "(null)".
std::string String::ShowWideCString(const wchar_t* wide_c_str) {
  if (wide_c_str == nullptr) return "(null)";

  return internal::WideStringToUtf8(wide_c_str, -1);
}

// Compares two wide C strings.  Returns true if and only if they have the
// same content.
//
// Unlike wcscmp(), this function can handle NULL argument(s).  A NULL
// C string is considered different to any non-NULL C string,
// including the empty string.
bool String::WideCStringEquals(const wchar_t* lhs, const wchar_t* rhs) {
  if (lhs == nullptr) return rhs == nullptr;

  if (rhs == nullptr) return false;

  return wcscmp(lhs, rhs) == 0;
}

// Helper function for *_STREQ on wide strings.
AssertionResult CmpHelperSTREQ(const char* lhs_expression,
                               const char* rhs_expression, const wchar_t* lhs,
                               const wchar_t* rhs) {
  if (String::WideCStringEquals(lhs, rhs)) {
    return AssertionSuccess();
  }

  return EqFailure(lhs_expression, rhs_expression, PrintToString(lhs),
                   PrintToString(rhs), false);
}

// Helper function for *_STRNE on wide strings.
AssertionResult CmpHelperSTRNE(const char* s1_expression,
                               const char* s2_expression, const wchar_t* s1,
                               const wchar_t* s2) {
  if (!String::WideCStringEquals(s1, s2)) {
    return AssertionSuccess();
  }

  return AssertionFailure()
         << "Expected: (" << s1_expression << ") != (" << s2_expression
         << "), actual: " << PrintToString(s1) << " vs " << PrintToString(s2);
}

// Compares two C strings, ignoring case.  Returns true if and only if they have
// the same content.
//
// Unlike strcasecmp(), this function can handle NULL argument(s).  A
// NULL C string is considered different to any non-NULL C string,
// including the empty string.
bool String::CaseInsensitiveCStringEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr) return rhs == nullptr;
  if (rhs == nullptr) return false;
  return posix::StrCaseCmp(lhs, rhs) == 0;
}

// Compares two wide C strings, ignoring case.  Returns true if and only if they
// have the same content.
//
// Unlike wcscasecmp(), this function can handle NULL argument(s).
// A NULL C string is considered different to any non-NULL wide C string,
// including the empty string.
// NB: The implementations on different platforms slightly differ.
// On windows, this method uses _wcsicmp which compares according to LC_CTYPE
// environment variable. On GNU platform this method uses wcscasecmp
// which compares according to LC_CTYPE category of the current locale.
// On MacOS X, it uses towlower, which also uses LC_CTYPE category of the
// current locale.
bool String::CaseInsensitiveWideCStringEquals(const wchar_t* lhs,
                                              const wchar_t* rhs) {
  if (lhs == nullptr) return rhs == nullptr;

  if (rhs == nullptr) return false;

#ifdef GTEST_OS_WINDOWS
  return _wcsicmp(lhs, rhs) == 0;
#elif defined(GTEST_OS_LINUX) && !defined(GTEST_OS_LINUX_ANDROID)
  return wcscasecmp(lhs, rhs) == 0;
#else
  // Android, Mac OS X and Cygwin don't define wcscasecmp.
  // Other unknown OSes may not define it either.
  wint_t left, right;
  do {
    left = towlower(static_cast<wint_t>(*lhs++));
    right = towlower(static_cast<wint_t>(*rhs++));
  } while (left && left == right);
  return left == right;
#endif  // OS selector
}

// Returns true if and only if str ends with the given suffix, ignoring case.
// Any string is considered to end with an empty suffix.
bool String::EndsWithCaseInsensitive(const std::string& str,
                                     const std::string& suffix) {
  const size_t str_len = str.length();
  const size_t suffix_len = suffix.length();
  return (str_len >= suffix_len) &&
         CaseInsensitiveCStringEquals(str.c_str() + str_len - suffix_len,
                                      suffix.c_str());
}

// Formats an int value as "%02d".
std::string String::FormatIntWidth2(int value) {
  return FormatIntWidthN(value, 2);
}

// Formats an int value to given width with leading zeros.
std::string String::FormatIntWidthN(int value, int width) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(width) << value;
  return ss.str();
}

// Formats an int value as "%X".
std::string String::FormatHexUInt32(uint32_t value) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << value;
  return ss.str();
}

// Formats an int value as "%X".
std::string String::FormatHexInt(int value) {
  return FormatHexUInt32(static_cast<uint32_t>(value));
}

// Formats a byte as "%02X".
std::string String::FormatByte(unsigned char value) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << std::hex << std::uppercase
     << static_cast<unsigned int>(value);
  return ss.str();
}

// Converts the buffer in a stringstream to an std::string, converting NUL
// bytes to "\\0" along the way.
std::string StringStreamToString(::std::stringstream* ss) {
  const ::std::string& str = ss->str();
  const char* const start = str.c_str();
  const char* const end = start + str.length();

  std::string result;
  result.reserve(static_cast<size_t>(2 * (end - start)));
  for (const char* ch = start; ch != end; ++ch) {
    if (*ch == '\0') {
      result += "\\0";  // Replaces NUL with "\\0";
    } else {
      result += *ch;
    }
  }

  return result;
}

// Appends the user-supplied message to the Google-Test-generated message.
std::string AppendUserMessage(const std::string& gtest_msg,
                              const Message& user_msg) {
  // Appends the user message if it's non-empty.
  const std::string user_msg_string = user_msg.GetString();
  if (user_msg_string.empty()) {
    return gtest_msg;
  }
  if (gtest_msg.empty()) {
    return user_msg_string;
  }
  return gtest_msg + "\n" + user_msg_string;
}

}  // namespace internal

// class TestResult

// Creates an empty TestResult.
TestResult::TestResult()
    : death_test_count_(0), start_timestamp_(0), elapsed_time_(0) {}

// D'tor.
TestResult::~TestResult() = default;

// Returns the i-th test part result among all the results. i can
// range from 0 to total_part_count() - 1. If i is not in that range,
// aborts the program.
const TestPartResult& TestResult::GetTestPartResult(int i) const {
  if (i < 0 || i >= total_part_count()) internal::posix::Abort();
  return test_part_results_.at(static_cast<size_t>(i));
}

// Returns the i-th test property. i can range from 0 to
// test_property_count() - 1. If i is not in that range, aborts the
// program.
const TestProperty& TestResult::GetTestProperty(int i) const {
  if (i < 0 || i >= test_property_count()) internal::posix::Abort();
  return test_properties_.at(static_cast<size_t>(i));
}

// Clears the test part results.
void TestResult::ClearTestPartResults() { test_part_results_.clear(); }

// Adds a test part result to the list.
void TestResult::AddTestPartResult(const TestPartResult& test_part_result) {
  test_part_results_.push_back(test_part_result);
}

// Adds a test property to the list. If a property with the same key as the
// supplied property is already represented, the value of this test_property
// replaces the old value for that key.
void TestResult::RecordProperty(const std::string& xml_element,
                                const TestProperty& test_property) {
  if (!ValidateTestProperty(xml_element, test_property)) {
    return;
  }
  internal::MutexLock lock(&test_properties_mutex_);
  const std::vector<TestProperty>::iterator property_with_matching_key =
      std::find_if(test_properties_.begin(), test_properties_.end(),
                   internal::TestPropertyKeyIs(test_property.key()));
  if (property_with_matching_key == test_properties_.end()) {
    test_properties_.push_back(test_property);
    return;
  }
  property_with_matching_key->SetValue(test_property.value());
}

// The list of reserved attributes used in the <testsuites> element of XML
// output.
static const char* const kReservedTestSuitesAttributes[] = {
    "disabled",    "errors", "failures", "name",
    "random_seed", "tests",  "time",     "timestamp"};

// The list of reserved attributes used in the <testsuite> element of XML
// output.
static const char* const kReservedTestSuiteAttributes[] = {
    "disabled", "errors", "failures",  "name",
    "tests",    "time",   "timestamp", "skipped"};

// The list of reserved attributes used in the <testcase> element of XML output.
static const char* const kReservedTestCaseAttributes[] = {
    "classname",  "name",        "status", "time",
    "type_param", "value_param", "file",   "line"};

// Use a slightly different set for allowed output to ensure existing tests can
// still RecordProperty("result") or RecordProperty("timestamp")
static const char* const kReservedOutputTestCaseAttributes[] = {
    "classname",   "name", "status", "time",   "type_param",
    "value_param", "file", "line",   "result", "timestamp"};

template <size_t kSize>
std::vector<std::string> ArrayAsVector(const char* const (&array)[kSize]) {
  return std::vector<std::string>(array, array + kSize);
}

static std::vector<std::string> GetReservedAttributesForElement(
    const std::string& xml_element) {
  if (xml_element == "testsuites") {
    return ArrayAsVector(kReservedTestSuitesAttributes);
  } else if (xml_element == "testsuite") {
    return ArrayAsVector(kReservedTestSuiteAttributes);
  } else if (xml_element == "testcase") {
    return ArrayAsVector(kReservedTestCaseAttributes);
  } else {
    GTEST_CHECK_(false) << "Unrecognized xml_element provided: " << xml_element;
  }
  // This code is unreachable but some compilers may not realizes that.
  return std::vector<std::string>();
}

#if GTEST_HAS_FILE_SYSTEM
// TODO(jdesprez): Merge the two getReserved attributes once skip is improved
// This function is only used when file systems are enabled.
static std::vector<std::string> GetReservedOutputAttributesForElement(
    const std::string& xml_element) {
  if (xml_element == "testsuites") {
    return ArrayAsVector(kReservedTestSuitesAttributes);
  } else if (xml_element == "testsuite") {
    return ArrayAsVector(kReservedTestSuiteAttributes);
  } else if (xml_element == "testcase") {
    return ArrayAsVector(kReservedOutputTestCaseAttributes);
  } else {
    GTEST_CHECK_(false) << "Unrecognized xml_element provided: " << xml_element;
  }
  // This code is unreachable but some compilers may not realizes that.
  return std::vector<std::string>();
}
#endif

static std::string FormatWordList(const std::vector<std::string>& words) {
  Message word_list;
  for (size_t i = 0; i < words.size(); ++i) {
    if (i > 0 && words.size() > 2) {
      word_list << ", ";
    }
    if (i == words.size() - 1) {
      word_list << "and ";
    }
    word_list << "'" << words[i] << "'";
  }
  return word_list.GetString();
}

static bool ValidateTestPropertyName(
    const std::string& property_name,
    const std::vector<std::string>& reserved_names) {
  if (std::find(reserved_names.begin(), reserved_names.end(), property_name) !=
      reserved_names.end()) {
    ADD_FAILURE() << "Reserved key used in RecordProperty(): " << property_name
                  << " (" << FormatWordList(reserved_names)
                  << " are reserved by " << GTEST_NAME_ << ")";
    return false;
  }
  return true;
}

// Adds a failure if the key is a reserved attribute of the element named
// xml_element.  Returns true if the property is valid.
bool TestResult::ValidateTestProperty(const std::string& xml_element,
                                      const TestProperty& test_property) {
  return ValidateTestPropertyName(test_property.key(),
                                  GetReservedAttributesForElement(xml_element));
}

// Clears the object.
void TestResult::Clear() {
  test_part_results_.clear();
  test_properties_.clear();
  death_test_count_ = 0;
  elapsed_time_ = 0;
}

// Returns true off the test part was skipped.
static bool TestPartSkipped(const TestPartResult& result) {
  return result.skipped();
}

// Returns true if and only if the test was skipped.
bool TestResult::Skipped() const {
  return !Failed() && CountIf(test_part_results_, TestPartSkipped) > 0;
}

// Returns true if and only if the test failed.
bool TestResult::Failed() const {
  for (int i = 0; i < total_part_count(); ++i) {
    if (GetTestPartResult(i).failed()) return true;
  }
  return false;
}

// Returns true if and only if the test part fatally failed.
static bool TestPartFatallyFailed(const TestPartResult& result) {
  return result.fatally_failed();
}

// Returns true if and only if the test fatally failed.
bool TestResult::HasFatalFailure() const {
  return CountIf(test_part_results_, TestPartFatallyFailed) > 0;
}

// Returns true if and only if the test part non-fatally failed.
static bool TestPartNonfatallyFailed(const TestPartResult& result) {
  return result.nonfatally_failed();
}

// Returns true if and only if the test has a non-fatal failure.
bool TestResult::HasNonfatalFailure() const {
  return CountIf(test_part_results_, TestPartNonfatallyFailed) > 0;
}

// Gets the number of all test parts.  This is the sum of the number
// of successful test parts and the number of failed test parts.
int TestResult::total_part_count() const {
  return static_cast<int>(test_part_results_.size());
}

// Returns the number of the test properties.
int TestResult::test_property_count() const {
  return static_cast<int>(test_properties_.size());
}

// class Test

// Creates a Test object.

// The c'tor saves the states of all flags.
Test::Test() : gtest_flag_saver_(new GTEST_FLAG_SAVER_) {}

// The d'tor restores the states of all flags.  The actual work is
// done by the d'tor of the gtest_flag_saver_ field, and thus not
// visible here.
Test::~Test() = default;

// Sets up the test fixture.
//
// A sub-class may override this.
void Test::SetUp() {}

// Tears down the test fixture.
//
// A sub-class may override this.
void Test::TearDown() {}

// Allows user supplied key value pairs to be recorded for later output.
void Test::RecordProperty(const std::string& key, const std::string& value) {
  UnitTest::GetInstance()->RecordProperty(key, value);
}

namespace internal {

void ReportFailureInUnknownLocation(TestPartResult::Type result_type,
                                    const std::string& message) {
  // This function is a friend of UnitTest and as such has access to
  // AddTestPartResult.
  UnitTest::GetInstance()->AddTestPartResult(
      result_type,
      nullptr,  // No info about the source file where the exception occurred.
      -1,       // We have no info on which line caused the exception.
      message,
      "");  // No stack trace, either.
}

}  // namespace internal

// Google Test requires all tests in the same test suite to use the same test
// fixture class.  This function checks if the current test has the
// same fixture class as the first test in the current test suite.  If
// yes, it returns true; otherwise it generates a Google Test failure and
// returns false.
bool Test::HasSameFixtureClass() {
  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  const TestSuite* const test_suite = impl->current_test_suite();

  // Info about the first test in the current test suite.
  const TestInfo* const first_test_info = test_suite->test_info_list()[0];
  const internal::TypeId first_fixture_id = first_test_info->fixture_class_id_;
  const char* const first_test_name = first_test_info->name();

  // Info about the current test.
  const TestInfo* const this_test_info = impl->current_test_info();
  const internal::TypeId this_fixture_id = this_test_info->fixture_class_id_;
  const char* const this_test_name = this_test_info->name();

  if (this_fixture_id != first_fixture_id) {
    // Is the first test defined using TEST?
    const bool first_is_TEST = first_fixture_id == internal::GetTestTypeId();
    // Is this test defined using TEST?
    const bool this_is_TEST = this_fixture_id == internal::GetTestTypeId();

    if (first_is_TEST || this_is_TEST) {
      // Both TEST and TEST_F appear in same test suite, which is incorrect.
      // Tell the user how to fix this.

      // Gets the name of the TEST and the name of the TEST_F.  Note
      // that first_is_TEST and this_is_TEST cannot both be true, as
      // the fixture IDs are different for the two tests.
      const char* const TEST_name =
          first_is_TEST ? first_test_name : this_test_name;
      const char* const TEST_F_name =
          first_is_TEST ? this_test_name : first_test_name;

      ADD_FAILURE()
          << "All tests in the same test suite must use the same test fixture\n"
          << "class, so mixing TEST_F and TEST in the same test suite is\n"
          << "illegal.  In test suite " << this_test_info->test_suite_name()
          << ",\n"
          << "test " << TEST_F_name << " is defined using TEST_F but\n"
          << "test " << TEST_name << " is defined using TEST.  You probably\n"
          << "want to change the TEST to TEST_F or move it to another test\n"
          << "case.";
    } else {
      // Two fixture classes with the same name appear in two different
      // namespaces, which is not allowed. Tell the user how to fix this.
      ADD_FAILURE()
          << "All tests in the same test suite must use the same test fixture\n"
          << "class.  However, in test suite "
          << this_test_info->test_suite_name() << ",\n"
          << "you defined test " << first_test_name << " and test "
          << this_test_name << "\n"
          << "using two different test fixture classes.  This can happen if\n"
          << "the two classes are from different namespaces or translation\n"
          << "units and have the same name.  You should probably rename one\n"
          << "of the classes to put the tests into different test suites.";
    }
    return false;
  }

  return true;
}

namespace internal {

#if GTEST_HAS_EXCEPTIONS

// Adds an "exception thrown" fatal failure to the current test.
static std::string FormatCxxExceptionMessage(const char* description,
                                             const char* location) {
  Message message;
  if (description != nullptr) {
    message << "C++ exception with description \"" << description << "\"";
  } else {
    message << "Unknown C++ exception";
  }
  message << " thrown in " << location << ".";

  return message.GetString();
}

static std::string PrintTestPartResultToString(
    const TestPartResult& test_part_result);

GoogleTestFailureException::GoogleTestFailureException(
    const TestPartResult& failure)
    : ::std::runtime_error(PrintTestPartResultToString(failure).c_str()) {}

#endif  // GTEST_HAS_EXCEPTIONS

// We put these helper functions in the internal namespace as IBM's xlC
// compiler rejects the code if they were declared static.

// Runs the given method and handles SEH exceptions it throws, when
// SEH is supported; returns the 0-value for type Result in case of an
// SEH exception.  (Microsoft compilers cannot handle SEH and C++
// exceptions in the same function.  Therefore, we provide a separate
// wrapper function for handling SEH exceptions.)
template <class T, typename Result>
Result HandleSehExceptionsInMethodIfSupported(T* object, Result (T::*method)(),
                                              const char* location) {
#if GTEST_HAS_SEH
  __try {
    return (object->*method)();
  } __except (internal::UnitTestOptions::GTestProcessSEH(  // NOLINT
      GetExceptionCode(), location)) {
    return static_cast<Result>(0);
  }
#else
  (void)location;
  return (object->*method)();
#endif  // GTEST_HAS_SEH
}

// Runs the given method and catches and reports C++ and/or SEH-style
// exceptions, if they are supported; returns the 0-value for type
// Result in case of an SEH exception.
template <class T, typename Result>
Result HandleExceptionsInMethodIfSupported(T* object, Result (T::*method)(),
                                           const char* location) {
  // NOTE: The user code can affect the way in which Google Test handles
  // exceptions by setting GTEST_FLAG(catch_exceptions), but only before
  // RUN_ALL_TESTS() starts. It is technically possible to check the flag
  // after the exception is caught and either report or re-throw the
  // exception based on the flag's value:
  //
  // try {
  //   // Perform the test method.
  // } catch (...) {
  //   if (GTEST_FLAG_GET(catch_exceptions))
  //     // Report the exception as failure.
  //   else
  //     throw;  // Re-throws the original exception.
  // }
  //
  // However, the purpose of this flag is to allow the program to drop into
  // the debugger when the exception is thrown. On most platforms, once the
  // control enters the catch block, the exception origin information is
  // lost and the debugger will stop the program at the point of the
  // re-throw in this function -- instead of at the point of the original
  // throw statement in the code under test.  For this reason, we perform
  // the check early, sacrificing the ability to affect Google Test's
  // exception handling in the method where the exception is thrown.
  if (internal::GetUnitTestImpl()->catch_exceptions()) {
#if GTEST_HAS_EXCEPTIONS
    try {
      return HandleSehExceptionsInMethodIfSupported(object, method, location);
    } catch (const AssertionException&) {  // NOLINT
      // This failure was reported already.
    } catch (const internal::GoogleTestFailureException&) {  // NOLINT
      // This exception type can only be thrown by a failed Google
      // Test assertion with the intention of letting another testing
      // framework catch it.  Therefore we just re-throw it.
      throw;
    } catch (const std::exception& e) {  // NOLINT
      internal::ReportFailureInUnknownLocation(
          TestPartResult::kFatalFailure,
          FormatCxxExceptionMessage(e.what(), location));
    } catch (...) {  // NOLINT
      internal::ReportFailureInUnknownLocation(
          TestPartResult::kFatalFailure,
          FormatCxxExceptionMessage(nullptr, location));
    }
    return static_cast<Result>(0);
#else
    return HandleSehExceptionsInMethodIfSupported(object, method, location);
#endif  // GTEST_HAS_EXCEPTIONS
  } else {
    return (object->*method)();
  }
}

}  // namespace internal

// Runs the test and updates the test result.
void Test::Run() {
  if (!HasSameFixtureClass()) return;

  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  impl->os_stack_trace_getter()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(this, &Test::SetUp, "SetUp()");
  // We will run the test only if SetUp() was successful and didn't call
  // GTEST_SKIP().
  if (!HasFatalFailure() && !IsSkipped()) {
    impl->os_stack_trace_getter()->UponLeavingGTest();
    internal::HandleExceptionsInMethodIfSupported(this, &Test::TestBody,
                                                  "the test body");
  }

  // However, we want to clean up as much as possible.  Hence we will
  // always call TearDown(), even if SetUp() or the test body has
  // failed.
  impl->os_stack_trace_getter()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(this, &Test::TearDown,
                                                "TearDown()");
}

// Returns true if and only if the current test has a fatal failure.
bool Test::HasFatalFailure() {
  return internal::GetUnitTestImpl()->current_test_result()->HasFatalFailure();
}

// Returns true if and only if the current test has a non-fatal failure.
bool Test::HasNonfatalFailure() {
  return internal::GetUnitTestImpl()
      ->current_test_result()
      ->HasNonfatalFailure();
}

// Returns true if and only if the current test was skipped.
bool Test::IsSkipped() {
  return internal::GetUnitTestImpl()->current_test_result()->Skipped();
}

// class TestInfo

// Constructs a TestInfo object. It assumes ownership of the test factory
// object.
TestInfo::TestInfo(std::string a_test_suite_name, std::string a_name,
                   const char* a_type_param, const char* a_value_param,
                   internal::CodeLocation a_code_location,
                   internal::TypeId fixture_class_id,
                   internal::TestFactoryBase* factory)
    : test_suite_name_(std::move(a_test_suite_name)),
      name_(std::move(a_name)),
      type_param_(a_type_param ? new std::string(a_type_param) : nullptr),
      value_param_(a_value_param ? new std::string(a_value_param) : nullptr),
      location_(std::move(a_code_location)),
      fixture_class_id_(fixture_class_id),
      should_run_(false),
      is_disabled_(false),
      matches_filter_(false),
      is_in_another_shard_(false),
      factory_(factory),
      result_() {}

// Destructs a TestInfo object.
TestInfo::~TestInfo() { delete factory_; }

namespace internal {

// Creates a new TestInfo object and registers it with Google Test;
// returns the created object.
//
// Arguments:
//
//   test_suite_name:  name of the test suite
//   name:             name of the test
//   type_param:       the name of the test's type parameter, or NULL if
//                     this is not a typed or a type-parameterized test.
//   value_param:      text representation of the test's value parameter,
//                     or NULL if this is not a value-parameterized test.
//   code_location:    code location where the test is defined
//   fixture_class_id: ID of the test fixture class
//   set_up_tc:        pointer to the function that sets up the test suite
//   tear_down_tc:     pointer to the function that tears down the test suite
//   factory:          pointer to the factory that creates a test object.
//                     The newly created TestInfo instance will assume
//                     ownership of the factory object.
TestInfo* MakeAndRegisterTestInfo(
    std::string test_suite_name, const char* name, const char* type_param,
    const char* value_param, CodeLocation code_location,
    TypeId fixture_class_id, SetUpTestSuiteFunc set_up_tc,
    TearDownTestSuiteFunc tear_down_tc, TestFactoryBase* factory) {
  TestInfo* const test_info =
      new TestInfo(std::move(test_suite_name), name, type_param, value_param,
                   std::move(code_location), fixture_class_id, factory);
  GetUnitTestImpl()->AddTestInfo(set_up_tc, tear_down_tc, test_info);
  return test_info;
}

void ReportInvalidTestSuiteType(const char* test_suite_name,
                                const CodeLocation& code_location) {
  Message errors;
  errors
      << "Attempted redefinition of test suite " << test_suite_name << ".\n"
      << "All tests in the same test suite must use the same test fixture\n"
      << "class.  However, in test suite " << test_suite_name << ", you tried\n"
      << "to define a test using a fixture class different from the one\n"
      << "used earlier. This can happen if the two fixture classes are\n"
      << "from different namespaces and have the same name. You should\n"
      << "probably rename one of the classes to put the tests into different\n"
      << "test suites.";

  GTEST_LOG_(ERROR) << FormatFileLocation(code_location.file.c_str(),
                                          code_location.line)
                    << " " << errors.GetString();
}

// This method expands all parameterized tests registered with macros TEST_P
// and INSTANTIATE_TEST_SUITE_P into regular tests and registers those.
// This will be done just once during the program runtime.
void UnitTestImpl::RegisterParameterizedTests() {
  if (!parameterized_tests_registered_) {
    parameterized_test_registry_.RegisterTests();
    type_parameterized_test_registry_.CheckForInstantiations();
    parameterized_tests_registered_ = true;
  }
}

}  // namespace internal

// Creates the test object, runs it, records its result, and then
// deletes it.
void TestInfo::Run() {
  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();
  if (!should_run_) {
    if (is_disabled_ && matches_filter_) repeater->OnTestDisabled(*this);
    return;
  }

  // Tells UnitTest where to store test result.
  UnitTest::GetInstance()->set_current_test_info(this);

  // Notifies the unit test event listeners that a test is about to start.
  repeater->OnTestStart(*this);
  result_.set_start_timestamp(internal::GetTimeInMillis());
  internal::Timer timer;
  UnitTest::GetInstance()->UponLeavingGTest();

  // Creates the test object.
  Test* const test = internal::HandleExceptionsInMethodIfSupported(
      factory_, &internal::TestFactoryBase::CreateTest,
      "the test fixture's constructor");

  // Runs the test if the constructor didn't generate a fatal failure or invoke
  // GTEST_SKIP().
  // Note that the object will not be null
  if (!Test::HasFatalFailure() && !Test::IsSkipped()) {
    // This doesn't throw as all user code that can throw are wrapped into
    // exception handling code.
    test->Run();
  }

  if (test != nullptr) {
    // Deletes the test object.
    UnitTest::GetInstance()->UponLeavingGTest();
    internal::HandleExceptionsInMethodIfSupported(
        test, &Test::DeleteSelf_, "the test fixture's destructor");
  }

  result_.set_elapsed_time(timer.Elapsed());

  // Notifies the unit test event listener that a test has just finished.
  repeater->OnTestEnd(*this);

  // Tells UnitTest to stop associating assertion results to this
  // test.
  UnitTest::GetInstance()->set_current_test_info(nullptr);
}

// Skip and records a skipped test result for this object.
void TestInfo::Skip() {
  if (!should_run_) return;

  UnitTest::GetInstance()->set_current_test_info(this);

  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();

  // Notifies the unit test event listeners that a test is about to start.
  repeater->OnTestStart(*this);

  const TestPartResult test_part_result =
      TestPartResult(TestPartResult::kSkip, this->file(), this->line(), "");
  internal::GetUnitTestImpl()
      ->GetTestPartResultReporterForCurrentThread()
      ->ReportTestPartResult(test_part_result);

  // Notifies the unit test event listener that a test has just finished.
  repeater->OnTestEnd(*this);
  UnitTest::GetInstance()->set_current_test_info(nullptr);
}

// class TestSuite

// Gets the number of successful tests in this test suite.
int TestSuite::successful_test_count() const {
  return CountIf(test_info_list_, TestPassed);
}

// Gets the number of successful tests in this test suite.
int TestSuite::skipped_test_count() const {
  return CountIf(test_info_list_, TestSkipped);
}

// Gets the number of failed tests in this test suite.
int TestSuite::failed_test_count() const {
  return CountIf(test_info_list_, TestFailed);
}

// Gets the number of disabled tests that will be reported in the XML report.
int TestSuite::reportable_disabled_test_count() const {
  return CountIf(test_info_list_, TestReportableDisabled);
}

// Gets the number of disabled tests in this test suite.
int TestSuite::disabled_test_count() const {
  return CountIf(test_info_list_, TestDisabled);
}

// Gets the number of tests to be printed in the XML report.
int TestSuite::reportable_test_count() const {
  return CountIf(test_info_list_, TestReportable);
}

// Get the number of tests in this test suite that should run.
int TestSuite::test_to_run_count() const {
  return CountIf(test_info_list_, ShouldRunTest);
}

// Gets the number of all tests.
int TestSuite::total_test_count() const {
  return static_cast<int>(test_info_list_.size());
}

// Creates a TestSuite with the given name.
//
// Arguments:
//
//   a_name:       name of the test suite
//   a_type_param: the name of the test suite's type parameter, or NULL if
//                 this is not a typed or a type-parameterized test suite.
//   set_up_tc:    pointer to the function that sets up the test suite
//   tear_down_tc: pointer to the function that tears down the test suite
TestSuite::TestSuite(const std::string& a_name, const char* a_type_param,
                     internal::SetUpTestSuiteFunc set_up_tc,
                     internal::TearDownTestSuiteFunc tear_down_tc)
    : name_(a_name),
      type_param_(a_type_param ? new std::string(a_type_param) : nullptr),
      set_up_tc_(set_up_tc),
      tear_down_tc_(tear_down_tc),
      should_run_(false),
      start_timestamp_(0),
      elapsed_time_(0) {}

// Destructor of TestSuite.
TestSuite::~TestSuite() {
  // Deletes every Test in the collection.
  ForEach(test_info_list_, internal::Delete<TestInfo>);
}

// Returns the i-th test among all the tests. i can range from 0 to
// total_test_count() - 1. If i is not in that range, returns NULL.
const TestInfo* TestSuite::GetTestInfo(int i) const {
  const int index = GetElementOr(test_indices_, i, -1);
  return index < 0 ? nullptr : test_info_list_[static_cast<size_t>(index)];
}

// Returns the i-th test among all the tests. i can range from 0 to
// total_test_count() - 1. If i is not in that range, returns NULL.
TestInfo* TestSuite::GetMutableTestInfo(int i) {
  const int index = GetElementOr(test_indices_, i, -1);
  return index < 0 ? nullptr : test_info_list_[static_cast<size_t>(index)];
}

// Adds a test to this test suite.  Will delete the test upon
// destruction of the TestSuite object.
void TestSuite::AddTestInfo(TestInfo* test_info) {
  test_info_list_.push_back(test_info);
  test_indices_.push_back(static_cast<int>(test_indices_.size()));
}

// Runs every test in this TestSuite.
void TestSuite::Run() {
  if (!should_run_) return;

  UnitTest::GetInstance()->set_current_test_suite(this);

  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();

  // Ensure our tests are in a deterministic order.
  //
  // We do this by sorting lexicographically on (file, line number), providing
  // an order matching what the user can see in the source code.
  //
  // In the common case the line number comparison shouldn't be necessary,
  // because the registrations made by the TEST macro are executed in order
  // within a translation unit. But this is not true of the manual registration
  // API, and in more exotic scenarios a single file may be part of multiple
  // translation units.
  std::stable_sort(test_info_list_.begin(), test_info_list_.end(),
                   [](const TestInfo* const a, const TestInfo* const b) {
                     if (const int result = std::strcmp(a->file(), b->file())) {
                       return result < 0;
                     }

                     return a->line() < b->line();
                   });

  // Call both legacy and the new API
  repeater->OnTestSuiteStart(*this);
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseStart(*this);
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_

  UnitTest::GetInstance()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(
      this, &TestSuite::RunSetUpTestSuite, "SetUpTestSuite()");

  const bool skip_all =
      ad_hoc_test_result().Failed() || ad_hoc_test_result().Skipped();

  start_timestamp_ = internal::GetTimeInMillis();
  internal::Timer timer;
  for (int i = 0; i < total_test_count(); i++) {
    if (skip_all) {
      GetMutableTestInfo(i)->Skip();
    } else {
      GetMutableTestInfo(i)->Run();
    }
    if (GTEST_FLAG_GET(fail_fast) &&
        GetMutableTestInfo(i)->result()->Failed()) {
      for (int j = i + 1; j < total_test_count(); j++) {
        GetMutableTestInfo(j)->Skip();
      }
      break;
    }
  }
  elapsed_time_ = timer.Elapsed();

  UnitTest::GetInstance()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(
      this, &TestSuite::RunTearDownTestSuite, "TearDownTestSuite()");

  // Call both legacy and the new API
  repeater->OnTestSuiteEnd(*this);
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseEnd(*this);
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_

  UnitTest::GetInstance()->set_current_test_suite(nullptr);
}

// Skips all tests under this TestSuite.
void TestSuite::Skip() {
  if (!should_run_) return;

  UnitTest::GetInstance()->set_current_test_suite(this);

  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();

  // Call both legacy and the new API
  repeater->OnTestSuiteStart(*this);
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseStart(*this);
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_

  for (int i = 0; i < total_test_count(); i++) {
    GetMutableTestInfo(i)->Skip();
  }

  // Call both legacy and the new API
  repeater->OnTestSuiteEnd(*this);
  // Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseEnd(*this);
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_

  UnitTest::GetInstance()->set_current_test_suite(nullptr);
}

// Clears the results of all tests in this test suite.
void TestSuite::ClearResult() {
  ad_hoc_test_result_.Clear();
  ForEach(test_info_list_, TestInfo::ClearTestResult);
}

// Shuffles the tests in this test suite.
void TestSuite::ShuffleTests(internal::Random* random) {
  Shuffle(random, &test_indices_);
}

// Restores the test order to before the first shuffle.
void TestSuite::UnshuffleTests() {
  for (size_t i = 0; i < test_indices_.size(); i++) {
    test_indices_[i] = static_cast<int>(i);
  }
}

// Formats a countable noun.  Depending on its quantity, either the
// singular form or the plural form is used. e.g.
//
// FormatCountableNoun(1, "formula", "formuli") returns "1 formula".
// FormatCountableNoun(5, "book", "books") returns "5 books".
static std::string FormatCountableNoun(int count, const char* singular_form,
                                       const char* plural_form) {
  return internal::StreamableToString(count) + " " +
         (count == 1 ? singular_form : plural_form);
}

// Formats the count of tests.
static std::string FormatTestCount(int test_count) {
  return FormatCountableNoun(test_count, "test", "tests");
}

// Formats the count of test suites.
static std::string FormatTestSuiteCount(int test_suite_count) {
  return FormatCountableNoun(test_suite_count, "test suite", "test suites");
}

// Converts a TestPartResult::Type enum to human-friendly string
// representation.  Both kNonFatalFailure and kFatalFailure are translated
// to "Failure", as the user usually doesn't care about the difference
// between the two when viewing the test result.
static const char* TestPartResultTypeToString(TestPartResult::Type type) {
  switch (type) {
    case TestPartResult::kSkip:
      return "Skipped\n";
    case TestPartResult::kSuccess:
      return "Success";

    case TestPartResult::kNonFatalFailure:
    case TestPartResult::kFatalFailure:
#ifdef _MSC_VER
      return "error: ";
#else
      return "Failure\n";
#endif
    default:
      return "Unknown result type";
  }
}

namespace internal {
namespace {
enum class GTestColor { kDefault, kRed, kGreen, kYellow };
}  // namespace

// Prints a TestPartResult to an std::string.
static std::string PrintTestPartResultToString(
    const TestPartResult& test_part_result) {
  return (Message() << internal::FormatFileLocation(
                           test_part_result.file_name(),
                           test_part_result.line_number())
                    << " "
                    << TestPartResultTypeToString(test_part_result.type())
                    << test_part_result.message())
      .GetString();
}

// Prints a TestPartResult.
static void PrintTestPartResult(const TestPartResult& test_part_result) {
  const std::string& result = PrintTestPartResultToString(test_part_result);
  printf("%s\n", result.c_str());
  fflush(stdout);
  // If the test program runs in Visual Studio or a debugger, the
  // following statements add the test part result message to the Output
  // window such that the user can double-click on it to jump to the
  // corresponding source code location; otherwise they do nothing.
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MOBILE)
  // We don't call OutputDebugString*() on Windows Mobile, as printing
  // to stdout is done by OutputDebugString() there already - we don't
  // want the same message printed twice.
  ::OutputDebugStringA(result.c_str());
  ::OutputDebugStringA("\n");
#endif
}

// class PrettyUnitTestResultPrinter
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MOBILE) &&       \
    !defined(GTEST_OS_WINDOWS_GAMES) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_MINGW)

// Returns the character attribute for the given color.
static WORD GetColorAttribute(GTestColor color) {
  switch (color) {
    case GTestColor::kRed:
      return FOREGROUND_RED;
    case GTestColor::kGreen:
      return FOREGROUND_GREEN;
    case GTestColor::kYellow:
      return FOREGROUND_RED | FOREGROUND_GREEN;
    default:
      return 0;
  }
}

static int GetBitOffset(WORD color_mask) {
  if (color_mask == 0) return 0;

  int bitOffset = 0;
  while ((color_mask & 1) == 0) {
    color_mask >>= 1;
    ++bitOffset;
  }
  return bitOffset;
}

static WORD GetNewColor(GTestColor color, WORD old_color_attrs) {
  // Let's reuse the BG
  static const WORD background_mask = BACKGROUND_BLUE | BACKGROUND_GREEN |
                                      BACKGROUND_RED | BACKGROUND_INTENSITY;
  static const WORD foreground_mask = FOREGROUND_BLUE | FOREGROUND_GREEN |
                                      FOREGROUND_RED | FOREGROUND_INTENSITY;
  const WORD existing_bg = old_color_attrs & background_mask;

  WORD new_color =
      GetColorAttribute(color) | existing_bg | FOREGROUND_INTENSITY;
  static const int bg_bitOffset = GetBitOffset(background_mask);
  static const int fg_bitOffset = GetBitOffset(foreground_mask);

  if (((new_color & background_mask) >> bg_bitOffset) ==
      ((new_color & foreground_mask) >> fg_bitOffset)) {
    new_color ^= FOREGROUND_INTENSITY;  // invert intensity
  }
  return new_color;
}

#else

// Returns the ANSI color code for the given color. GTestColor::kDefault is
// an invalid input.
static const char* GetAnsiColorCode(GTestColor color) {
  switch (color) {
    case GTestColor::kRed:
      return "1";
    case GTestColor::kGreen:
      return "2";
    case GTestColor::kYellow:
      return "3";
    default:
      assert(false);
      return "9";
  }
}

#endif  // GTEST_OS_WINDOWS && !GTEST_OS_WINDOWS_MOBILE

// Returns true if and only if Google Test should use colors in the output.
bool ShouldUseColor(bool stdout_is_tty) {
  std::string c = GTEST_FLAG_GET(color);
  const char* const gtest_color = c.c_str();

  if (String::CaseInsensitiveCStringEquals(gtest_color, "auto")) {
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MINGW)
    // On Windows the TERM variable is usually not set, but the
    // console there does support colors.
    return stdout_is_tty;
#else
    // On non-Windows platforms, we rely on the TERM variable.
    const char* const term = posix::GetEnv("TERM");
    const bool term_supports_color =
        term != nullptr && (String::CStringEquals(term, "xterm") ||
                            String::CStringEquals(term, "xterm-color") ||
                            String::CStringEquals(term, "xterm-kitty") ||
                            String::CStringEquals(term, "alacritty") ||
                            String::CStringEquals(term, "screen") ||
                            String::CStringEquals(term, "tmux") ||
                            String::CStringEquals(term, "rxvt-unicode") ||
                            String::CStringEquals(term, "linux") ||
                            String::CStringEquals(term, "cygwin") ||
                            String::EndsWithCaseInsensitive(term, "-256color"));
    return stdout_is_tty && term_supports_color;
#endif  // GTEST_OS_WINDOWS
  }

  return String::CaseInsensitiveCStringEquals(gtest_color, "yes") ||
         String::CaseInsensitiveCStringEquals(gtest_color, "true") ||
         String::CaseInsensitiveCStringEquals(gtest_color, "t") ||
         String::CStringEquals(gtest_color, "1");
  // We take "yes", "true", "t", and "1" as meaning "yes".  If the
  // value is neither one of these nor "auto", we treat it as "no" to
  // be conservative.
}

// Helpers for printing colored strings to stdout. Note that on Windows, we
// cannot simply emit special characters and have the terminal change colors.
// This routine must actually emit the characters rather than return a string
// that would be colored when printed, as can be done on Linux.

GTEST_ATTRIBUTE_PRINTF_(2, 3)
static void ColoredPrintf(GTestColor color, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  static const bool in_color_mode =
      // We don't condition this on GTEST_HAS_FILE_SYSTEM because we still need
      // to be able to detect terminal I/O regardless.
      ShouldUseColor(posix::IsATTY(posix::FileNo(stdout)) != 0);

  const bool use_color = in_color_mode && (color != GTestColor::kDefault);

  if (!use_color) {
    vprintf(fmt, args);
    va_end(args);
    return;
  }

#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MOBILE) &&       \
    !defined(GTEST_OS_WINDOWS_GAMES) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_MINGW)
  const HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

  // Gets the current text color.
  CONSOLE_SCREEN_BUFFER_INFO buffer_info;
  GetConsoleScreenBufferInfo(stdout_handle, &buffer_info);
  const WORD old_color_attrs = buffer_info.wAttributes;
  const WORD new_color = GetNewColor(color, old_color_attrs);

  // We need to flush the stream buffers into the console before each
  // SetConsoleTextAttribute call lest it affect the text that is already
  // printed but has not yet reached the console.
  fflush(stdout);
  SetConsoleTextAttribute(stdout_handle, new_color);

  vprintf(fmt, args);

  fflush(stdout);
  // Restores the text color.
  SetConsoleTextAttribute(stdout_handle, old_color_attrs);
#else
  printf("\033[0;3%sm", GetAnsiColorCode(color));
  vprintf(fmt, args);
  printf("\033[m");  // Resets the terminal to default.
#endif  // GTEST_OS_WINDOWS && !GTEST_OS_WINDOWS_MOBILE
  va_end(args);
}

// Text printed in Google Test's text output and --gtest_list_tests
// output to label the type parameter and value parameter for a test.
static const char kTypeParamLabel[] = "TypeParam";
static const char kValueParamLabel[] = "GetParam()";

static void PrintFullTestCommentIfPresent(const TestInfo& test_info) {
  const char* const type_param = test_info.type_param();
  const char* const value_param = test_info.value_param();

  if (type_param != nullptr || value_param != nullptr) {
    printf(", where ");
    if (type_param != nullptr) {
      printf("%s = %s", kTypeParamLabel, type_param);
      if (value_param != nullptr) printf(" and ");
    }
    if (value_param != nullptr) {
      printf("%s = %s", kValueParamLabel, value_param);
    }
  }
}

// This class implements the TestEventListener interface.
//
// Class PrettyUnitTestResultPrinter is copyable.
class PrettyUnitTestResultPrinter : public TestEventListener {
 public:
  PrettyUnitTestResultPrinter() = default;
  static void PrintTestName(const char* test_suite, const char* test) {
    printf("%s.%s", test_suite, test);
  }

  // The following methods override what's in the TestEventListener class.
  void OnTestProgramStart(const UnitTest& /*unit_test*/) override {}
  void OnTestIterationStart(const UnitTest& unit_test, int iteration) override;
  void OnEnvironmentsSetUpStart(const UnitTest& unit_test) override;
  void OnEnvironmentsSetUpEnd(const UnitTest& /*unit_test*/) override {}
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestCase& test_case) override;
#else
  void OnTestSuiteStart(const TestSuite& test_suite) override;
#endif  // OnTestCaseStart

  void OnTestStart(const TestInfo& test_info) override;
  void OnTestDisabled(const TestInfo& test_info) override;

  void OnTestPartResult(const TestPartResult& result) override;
  void OnTestEnd(const TestInfo& test_info) override;
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase& test_case) override;
#else
  void OnTestSuiteEnd(const TestSuite& test_suite) override;
#endif  // GTEST_REMOVE_LEGACY_TEST_CASEAPI_

  void OnEnvironmentsTearDownStart(const UnitTest& unit_test) override;
  void OnEnvironmentsTearDownEnd(const UnitTest& /*unit_test*/) override {}
  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void OnTestProgramEnd(const UnitTest& /*unit_test*/) override {}

 private:
  static void PrintFailedTests(const UnitTest& unit_test);
  static void PrintFailedTestSuites(const UnitTest& unit_test);
  static void PrintSkippedTests(const UnitTest& unit_test);
};

// Fired before each iteration of tests starts.
void PrettyUnitTestResultPrinter::OnTestIterationStart(
    const UnitTest& unit_test, int iteration) {
  if (GTEST_FLAG_GET(repeat) != 1)
    printf("\nRepeating all tests (iteration %d) . . .\n\n", iteration + 1);

  std::string f = GTEST_FLAG_GET(filter);
  const char* const filter = f.c_str();

  // Prints the filter if it's not *.  This reminds the user that some
  // tests may be skipped.
  if (!String::CStringEquals(filter, kUniversalFilter)) {
    ColoredPrintf(GTestColor::kYellow, "Note: %s filter = %s\n", GTEST_NAME_,
                  filter);
  }

  if (internal::ShouldShard(kTestTotalShards, kTestShardIndex, false)) {
    const int32_t shard_index = Int32FromEnvOrDie(kTestShardIndex, -1);
    ColoredPrintf(GTestColor::kYellow, "Note: This is test shard %d of %s.\n",
                  static_cast<int>(shard_index) + 1,
                  internal::posix::GetEnv(kTestTotalShards));
  }

  if (GTEST_FLAG_GET(shuffle)) {
    ColoredPrintf(GTestColor::kYellow,
                  "Note: Randomizing tests' orders with a seed of %d .\n",
                  unit_test.random_seed());
  }

  ColoredPrintf(GTestColor::kGreen, "[==========] ");
  printf("Running %s from %s.\n",
         FormatTestCount(unit_test.test_to_run_count()).c_str(),
         FormatTestSuiteCount(unit_test.test_suite_to_run_count()).c_str());
  fflush(stdout);
}

void PrettyUnitTestResultPrinter::OnEnvironmentsSetUpStart(
    const UnitTest& /*unit_test*/) {
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("Global test environment set-up.\n");
  fflush(stdout);
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
void PrettyUnitTestResultPrinter::OnTestCaseStart(const TestCase& test_case) {
  const std::string counts =
      FormatCountableNoun(test_case.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s", counts.c_str(), test_case.name());
  if (test_case.type_param() == nullptr) {
    printf("\n");
  } else {
    printf(", where %s = %s\n", kTypeParamLabel, test_case.type_param());
  }
  fflush(stdout);
}
#else
void PrettyUnitTestResultPrinter::OnTestSuiteStart(
    const TestSuite& test_suite) {
  const std::string counts =
      FormatCountableNoun(test_suite.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s", counts.c_str(), test_suite.name());
  if (test_suite.type_param() == nullptr) {
    printf("\n");
  } else {
    printf(", where %s = %s\n", kTypeParamLabel, test_suite.type_param());
  }
  fflush(stdout);
}
#endif  // GTEST_REMOVE_LEGACY_TEST_CASEAPI_

void PrettyUnitTestResultPrinter::OnTestStart(const TestInfo& test_info) {
  ColoredPrintf(GTestColor::kGreen, "[ RUN      ] ");
  PrintTestName(test_info.test_suite_name(), test_info.name());
  printf("\n");
  fflush(stdout);
}

void PrettyUnitTestResultPrinter::OnTestDisabled(const TestInfo& test_info) {
  ColoredPrintf(GTestColor::kYellow, "[ DISABLED ] ");
  PrintTestName(test_info.test_suite_name(), test_info.name());
  printf("\n");
  fflush(stdout);
}

// Called after an assertion failure.
void PrettyUnitTestResultPrinter::OnTestPartResult(
    const TestPartResult& result) {
  switch (result.type()) {
    // If the test part succeeded, we don't need to do anything.
    case TestPartResult::kSuccess:
      return;
    default:
      // Print failure message from the assertion
      // (e.g. expected this and got that).
      PrintTestPartResult(result);
      fflush(stdout);
  }
}

void PrettyUnitTestResultPrinter::OnTestEnd(const TestInfo& test_info) {
  if (test_info.result()->Passed()) {
    ColoredPrintf(GTestColor::kGreen, "[       OK ] ");
  } else if (test_info.result()->Skipped()) {
    ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
  } else {
    ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
  }
  PrintTestName(test_info.test_suite_name(), test_info.name());
  if (test_info.result()->Failed()) PrintFullTestCommentIfPresent(test_info);

  if (GTEST_FLAG_GET(print_time)) {
    printf(" (%s ms)\n",
           internal::StreamableToString(test_info.result()->elapsed_time())
               .c_str());
  } else {
    printf("\n");
  }
  fflush(stdout);
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
void PrettyUnitTestResultPrinter::OnTestCaseEnd(const TestCase& test_case) {
  if (!GTEST_FLAG_GET(print_time)) return;

  const std::string counts =
      FormatCountableNoun(test_case.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s (%s ms total)\n\n", counts.c_str(), test_case.name(),
         internal::StreamableToString(test_case.elapsed_time()).c_str());
  fflush(stdout);
}
#else
void PrettyUnitTestResultPrinter::OnTestSuiteEnd(const TestSuite& test_suite) {
  if (!GTEST_FLAG_GET(print_time)) return;

  const std::string counts =
      FormatCountableNoun(test_suite.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s (%s ms total)\n\n", counts.c_str(), test_suite.name(),
         internal::StreamableToString(test_suite.elapsed_time()).c_str());
  fflush(stdout);
}
#endif  // GTEST_REMOVE_LEGACY_TEST_CASEAPI_

void PrettyUnitTestResultPrinter::OnEnvironmentsTearDownStart(
    const UnitTest& /*unit_test*/) {
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("Global test environment tear-down\n");
  fflush(stdout);
}

// Internal helper for printing the list of failed tests.
void PrettyUnitTestResultPrinter::PrintFailedTests(const UnitTest& unit_test) {
  const int failed_test_count = unit_test.failed_test_count();
  ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
  printf("%s, listed below:\n", FormatTestCount(failed_test_count).c_str());

  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    const TestSuite& test_suite = *unit_test.GetTestSuite(i);
    if (!test_suite.should_run() || (test_suite.failed_test_count() == 0)) {
      continue;
    }
    for (int j = 0; j < test_suite.total_test_count(); ++j) {
      const TestInfo& test_info = *test_suite.GetTestInfo(j);
      if (!test_info.should_run() || !test_info.result()->Failed()) {
        continue;
      }
      ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
      printf("%s.%s", test_suite.name(), test_info.name());
      PrintFullTestCommentIfPresent(test_info);
      printf("\n");
    }
  }
  printf("\n%2d FAILED %s\n", failed_test_count,
         failed_test_count == 1 ? "TEST" : "TESTS");
}

// Internal helper for printing the list of test suite failures not covered by
// PrintFailedTests.
void PrettyUnitTestResultPrinter::PrintFailedTestSuites(
    const UnitTest& unit_test) {
  int suite_failure_count = 0;
  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    const TestSuite& test_suite = *unit_test.GetTestSuite(i);
    if (!test_suite.should_run()) {
      continue;
    }
    if (test_suite.ad_hoc_test_result().Failed()) {
      ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
      printf("%s: SetUpTestSuite or TearDownTestSuite\n", test_suite.name());
      ++suite_failure_count;
    }
  }
  if (suite_failure_count > 0) {
    printf("\n%2d FAILED TEST %s\n", suite_failure_count,
           suite_failure_count == 1 ? "SUITE" : "SUITES");
  }
}

// Internal helper for printing the list of skipped tests.
void PrettyUnitTestResultPrinter::PrintSkippedTests(const UnitTest& unit_test) {
  const int skipped_test_count = unit_test.skipped_test_count();
  if (skipped_test_count == 0) {
    return;
  }

  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    const TestSuite& test_suite = *unit_test.GetTestSuite(i);
    if (!test_suite.should_run() || (test_suite.skipped_test_count() == 0)) {
      continue;
    }
    for (int j = 0; j < test_suite.total_test_count(); ++j) {
      const TestInfo& test_info = *test_suite.GetTestInfo(j);
      if (!test_info.should_run() || !test_info.result()->Skipped()) {
        continue;
      }
      ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
      printf("%s.%s", test_suite.name(), test_info.name());
      printf("\n");
    }
  }
}

void PrettyUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                     int /*iteration*/) {
  ColoredPrintf(GTestColor::kGreen, "[==========] ");
  printf("%s from %s ran.",
         FormatTestCount(unit_test.test_to_run_count()).c_str(),
         FormatTestSuiteCount(unit_test.test_suite_to_run_count()).c_str());
  if (GTEST_FLAG_GET(print_time)) {
    printf(" (%s ms total)",
           internal::StreamableToString(unit_test.elapsed_time()).c_str());
  }
  printf("\n");
  ColoredPrintf(GTestColor::kGreen, "[  PASSED  ] ");
  printf("%s.\n", FormatTestCount(unit_test.successful_test_count()).c_str());

  const int skipped_test_count = unit_test.skipped_test_count();
  if (skipped_test_count > 0) {
    ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
    printf("%s, listed below:\n", FormatTestCount(skipped_test_count).c_str());
    PrintSkippedTests(unit_test);
  }

  if (!unit_test.Passed()) {
    PrintFailedTests(unit_test);
    PrintFailedTestSuites(unit_test);
  }

  int num_disabled = unit_test.reportable_disabled_test_count();
  if (num_disabled && !GTEST_FLAG_GET(also_run_disabled_tests)) {
    if (unit_test.Passed()) {
      printf("\n");  // Add a spacer if no FAILURE banner is displayed.
    }
    ColoredPrintf(GTestColor::kYellow, "  YOU HAVE %d DISABLED %s\n\n",
                  num_disabled, num_disabled == 1 ? "TEST" : "TESTS");
  }
  // Ensure that Google Test output is printed before, e.g., heapchecker output.
  fflush(stdout);
}

// End PrettyUnitTestResultPrinter

// This class implements the TestEventListener interface.
//
// Class BriefUnitTestResultPrinter is copyable.
class BriefUnitTestResultPrinter : public TestEventListener {
 public:
  BriefUnitTestResultPrinter() = default;
  static void PrintTestName(const char* test_suite, const char* test) {
    printf("%s.%s", test_suite, test);
  }

  // The following methods override what's in the TestEventListener class.
  void OnTestProgramStart(const UnitTest& /*unit_test*/) override {}
  void OnTestIterationStart(const UnitTest& /*unit_test*/,
                            int /*iteration*/) override {}
  void OnEnvironmentsSetUpStart(const UnitTest& /*unit_test*/) override {}
  void OnEnvironmentsSetUpEnd(const UnitTest& /*unit_test*/) override {}
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestCase& /*test_case*/) override {}
#else
  void OnTestSuiteStart(const TestSuite& /*test_suite*/) override {}
#endif  // OnTestCaseStart

  void OnTestStart(const TestInfo& /*test_info*/) override {}
  void OnTestDisabled(const TestInfo& /*test_info*/) override {}

  void OnTestPartResult(const TestPartResult& result) override;
  void OnTestEnd(const TestInfo& test_info) override;
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase& /*test_case*/) override {}
#else
  void OnTestSuiteEnd(const TestSuite& /*test_suite*/) override {}
#endif  // GTEST_REMOVE_LEGACY_TEST_CASEAPI_

  void OnEnvironmentsTearDownStart(const UnitTest& /*unit_test*/) override {}
  void OnEnvironmentsTearDownEnd(const UnitTest& /*unit_test*/) override {}
  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void OnTestProgramEnd(const UnitTest& /*unit_test*/) override {}
};

// Called after an assertion failure.
void BriefUnitTestResultPrinter::OnTestPartResult(
    const TestPartResult& result) {
  switch (result.type()) {
    // If the test part succeeded, we don't need to do anything.
    case TestPartResult::kSuccess:
      return;
    default:
      // Print failure message from the assertion
      // (e.g. expected this and got that).
      PrintTestPartResult(result);
      fflush(stdout);
  }
}

void BriefUnitTestResultPrinter::OnTestEnd(const TestInfo& test_info) {
  if (test_info.result()->Failed()) {
    ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
    PrintTestName(test_info.test_suite_name(), test_info.name());
    PrintFullTestCommentIfPresent(test_info);

    if (GTEST_FLAG_GET(print_time)) {
      printf(" (%s ms)\n",
             internal::StreamableToString(test_info.result()->elapsed_time())
                 .c_str());
    } else {
      printf("\n");
    }
    fflush(stdout);
  }
}

void BriefUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                    int /*iteration*/) {
  ColoredPrintf(GTestColor::kGreen, "[==========] ");
  printf("%s from %s ran.",
         FormatTestCount(unit_test.test_to_run_count()).c_str(),
         FormatTestSuiteCount(unit_test.test_suite_to_run_count()).c_str());
  if (GTEST_FLAG_GET(print_time)) {
    printf(" (%s ms total)",
           internal::StreamableToString(unit_test.elapsed_time()).c_str());
  }
  printf("\n");
  ColoredPrintf(GTestColor::kGreen, "[  PASSED  ] ");
  printf("%s.\n", FormatTestCount(unit_test.successful_test_count()).c_str());

  const int skipped_test_count = unit_test.skipped_test_count();
  if (skipped_test_count > 0) {
    ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
    printf("%s.\n", FormatTestCount(skipped_test_count).c_str());
  }

  int num_disabled = unit_test.reportable_disabled_test_count();
  if (num_disabled && !GTEST_FLAG_GET(also_run_disabled_tests)) {
    if (unit_test.Passed()) {
      printf("\n");  // Add a spacer if no FAILURE banner is displayed.
    }
    ColoredPrintf(GTestColor::kYellow, "  YOU HAVE %d DISABLED %s\n\n",
                  num_disabled, num_disabled == 1 ? "TEST" : "TESTS");
  }
  // Ensure that Google Test output is printed before, e.g., heapchecker output.
  fflush(stdout);
}

// End BriefUnitTestResultPrinter

// class TestEventRepeater
//
// This class forwards events to other event listeners.
class TestEventRepeater : public TestEventListener {
 public:
  TestEventRepeater() : forwarding_enabled_(true) {}
  ~TestEventRepeater() override;
  void Append(TestEventListener* listener);
  TestEventListener* Release(TestEventListener* listener);

  // Controls whether events will be forwarded to listeners_. Set to false
  // in death test child processes.
  bool forwarding_enabled() const { return forwarding_enabled_; }
  void set_forwarding_enabled(bool enable) { forwarding_enabled_ = enable; }

  void OnTestProgramStart(const UnitTest& parameter) override;
  void OnTestIterationStart(const UnitTest& unit_test, int iteration) override;
  void OnEnvironmentsSetUpStart(const UnitTest& parameter) override;
  void OnEnvironmentsSetUpEnd(const UnitTest& parameter) override;
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestSuite& parameter) override;
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestSuiteStart(const TestSuite& parameter) override;
  void OnTestStart(const TestInfo& parameter) override;
  void OnTestDisabled(const TestInfo& parameter) override;
  void OnTestPartResult(const TestPartResult& parameter) override;
  void OnTestEnd(const TestInfo& parameter) override;
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase& parameter) override;
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestSuiteEnd(const TestSuite& parameter) override;
  void OnEnvironmentsTearDownStart(const UnitTest& parameter) override;
  void OnEnvironmentsTearDownEnd(const UnitTest& parameter) override;
  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void OnTestProgramEnd(const UnitTest& parameter) override;

 private:
  // Controls whether events will be forwarded to listeners_. Set to false
  // in death test child processes.
  bool forwarding_enabled_;
  // The list of listeners that receive events.
  std::vector<TestEventListener*> listeners_;

  TestEventRepeater(const TestEventRepeater&) = delete;
  TestEventRepeater& operator=(const TestEventRepeater&) = delete;
};

TestEventRepeater::~TestEventRepeater() {
  ForEach(listeners_, Delete<TestEventListener>);
}

void TestEventRepeater::Append(TestEventListener* listener) {
  listeners_.push_back(listener);
}

TestEventListener* TestEventRepeater::Release(TestEventListener* listener) {
  for (size_t i = 0; i < listeners_.size(); ++i) {
    if (listeners_[i] == listener) {
      listeners_.erase(listeners_.begin() + static_cast<int>(i));
      return listener;
    }
  }

  return nullptr;
}

// Since most methods are very similar, use macros to reduce boilerplate.
// This defines a member that forwards the call to all listeners.
#define GTEST_REPEATER_METHOD_(Name, Type)              \
  void TestEventRepeater::Name(const Type& parameter) { \
    if (forwarding_enabled_) {                          \
      for (size_t i = 0; i < listeners_.size(); i++) {  \
        listeners_[i]->Name(parameter);                 \
      }                                                 \
    }                                                   \
  }
// This defines a member that forwards the call to all listeners in reverse
// order.
#define GTEST_REVERSE_REPEATER_METHOD_(Name, Type)      \
  void TestEventRepeater::Name(const Type& parameter) { \
    if (forwarding_enabled_) {                          \
      for (size_t i = listeners_.size(); i != 0; i--) { \
        listeners_[i - 1]->Name(parameter);             \
      }                                                 \
    }                                                   \
  }

GTEST_REPEATER_METHOD_(OnTestProgramStart, UnitTest)
GTEST_REPEATER_METHOD_(OnEnvironmentsSetUpStart, UnitTest)
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
GTEST_REPEATER_METHOD_(OnTestCaseStart, TestSuite)
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_
GTEST_REPEATER_METHOD_(OnTestSuiteStart, TestSuite)
GTEST_REPEATER_METHOD_(OnTestStart, TestInfo)
GTEST_REPEATER_METHOD_(OnTestDisabled, TestInfo)
GTEST_REPEATER_METHOD_(OnTestPartResult, TestPartResult)
GTEST_REPEATER_METHOD_(OnEnvironmentsTearDownStart, UnitTest)
GTEST_REVERSE_REPEATER_METHOD_(OnEnvironmentsSetUpEnd, UnitTest)
GTEST_REVERSE_REPEATER_METHOD_(OnEnvironmentsTearDownEnd, UnitTest)
GTEST_REVERSE_REPEATER_METHOD_(OnTestEnd, TestInfo)
//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
GTEST_REVERSE_REPEATER_METHOD_(OnTestCaseEnd, TestSuite)
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_
GTEST_REVERSE_REPEATER_METHOD_(OnTestSuiteEnd, TestSuite)
GTEST_REVERSE_REPEATER_METHOD_(OnTestProgramEnd, UnitTest)

#undef GTEST_REPEATER_METHOD_
#undef GTEST_REVERSE_REPEATER_METHOD_

void TestEventRepeater::OnTestIterationStart(const UnitTest& unit_test,
                                             int iteration) {
  if (forwarding_enabled_) {
    for (size_t i = 0; i < listeners_.size(); i++) {
      listeners_[i]->OnTestIterationStart(unit_test, iteration);
    }
  }
}

void TestEventRepeater::OnTestIterationEnd(const UnitTest& unit_test,
                                           int iteration) {
  if (forwarding_enabled_) {
    for (size_t i = listeners_.size(); i > 0; i--) {
      listeners_[i - 1]->OnTestIterationEnd(unit_test, iteration);
    }
  }
}

// End TestEventRepeater

#if GTEST_HAS_FILE_SYSTEM
// This class generates an XML output file.
class XmlUnitTestResultPrinter : public EmptyTestEventListener {
 public:
  explicit XmlUnitTestResultPrinter(const char* output_file);

  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void ListTestsMatchingFilter(const std::vector<TestSuite*>& test_suites);

  // Prints an XML summary of all unit tests.
  static void PrintXmlTestsList(std::ostream* stream,
                                const std::vector<TestSuite*>& test_suites);

 private:
  // Is c a whitespace character that is normalized to a space character
  // when it appears in an XML attribute value?
  static bool IsNormalizableWhitespace(unsigned char c) {
    return c == '\t' || c == '\n' || c == '\r';
  }

  // May c appear in a well-formed XML document?
  // https://www.w3.org/TR/REC-xml/#charsets
  static bool IsValidXmlCharacter(unsigned char c) {
    return IsNormalizableWhitespace(c) || c >= 0x20;
  }

  // Returns an XML-escaped copy of the input string str.  If
  // is_attribute is true, the text is meant to appear as an attribute
  // value, and normalizable whitespace is preserved by replacing it
  // with character references.
  static std::string EscapeXml(const std::string& str, bool is_attribute);

  // Returns the given string with all characters invalid in XML removed.
  static std::string RemoveInvalidXmlCharacters(const std::string& str);

  // Convenience wrapper around EscapeXml when str is an attribute value.
  static std::string EscapeXmlAttribute(const std::string& str) {
    return EscapeXml(str, true);
  }

  // Convenience wrapper around EscapeXml when str is not an attribute value.
  static std::string EscapeXmlText(const char* str) {
    return EscapeXml(str, false);
  }

  // Verifies that the given attribute belongs to the given element and
  // streams the attribute as XML.
  static void OutputXmlAttribute(std::ostream* stream,
                                 const std::string& element_name,
                                 const std::string& name,
                                 const std::string& value);

  // Streams an XML CDATA section, escaping invalid CDATA sequences as needed.
  static void OutputXmlCDataSection(::std::ostream* stream, const char* data);

  // Streams a test suite XML stanza containing the given test result.
  //
  // Requires: result.Failed()
  static void OutputXmlTestSuiteForTestResult(::std::ostream* stream,
                                              const TestResult& result);

  // Streams a test case XML stanza containing the given test result.
  //
  // Requires: result.Failed()
  static void OutputXmlTestCaseForTestResult(::std::ostream* stream,
                                             const TestResult& result);

  // Streams an XML representation of a TestResult object.
  static void OutputXmlTestResult(::std::ostream* stream,
                                  const TestResult& result);

  // Streams an XML representation of a TestInfo object.
  static void OutputXmlTestInfo(::std::ostream* stream,
                                const char* test_suite_name,
                                const TestInfo& test_info);

  // Prints an XML representation of a TestSuite object
  static void PrintXmlTestSuite(::std::ostream* stream,
                                const TestSuite& test_suite);

  // Prints an XML summary of unit_test to output stream out.
  static void PrintXmlUnitTest(::std::ostream* stream,
                               const UnitTest& unit_test);

  // Streams an XML representation of the test properties of a TestResult
  // object.
  static void OutputXmlTestProperties(std::ostream* stream,
                                      const TestResult& result,
                                      const std::string& indent);

  // The output file.
  const std::string output_file_;

  XmlUnitTestResultPrinter(const XmlUnitTestResultPrinter&) = delete;
  XmlUnitTestResultPrinter& operator=(const XmlUnitTestResultPrinter&) = delete;
};

// Creates a new XmlUnitTestResultPrinter.
XmlUnitTestResultPrinter::XmlUnitTestResultPrinter(const char* output_file)
    : output_file_(output_file) {
  if (output_file_.empty()) {
    GTEST_LOG_(FATAL) << "XML output file may not be null";
  }
}

// Called after the unit test ends.
void XmlUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                  int /*iteration*/) {
  FILE* xmlout = OpenFileForWriting(output_file_);
  std::stringstream stream;
  PrintXmlUnitTest(&stream, unit_test);
  fprintf(xmlout, "%s", StringStreamToString(&stream).c_str());
  fclose(xmlout);
}

void XmlUnitTestResultPrinter::ListTestsMatchingFilter(
    const std::vector<TestSuite*>& test_suites) {
  FILE* xmlout = OpenFileForWriting(output_file_);
  std::stringstream stream;
  PrintXmlTestsList(&stream, test_suites);
  fprintf(xmlout, "%s", StringStreamToString(&stream).c_str());
  fclose(xmlout);
}

// Returns an XML-escaped copy of the input string str.  If is_attribute
// is true, the text is meant to appear as an attribute value, and
// normalizable whitespace is preserved by replacing it with character
// references.
//
// Invalid XML characters in str, if any, are stripped from the output.
// It is expected that most, if not all, of the text processed by this
// module will consist of ordinary English text.
// If this module is ever modified to produce version 1.1 XML output,
// most invalid characters can be retained using character references.
std::string XmlUnitTestResultPrinter::EscapeXml(const std::string& str,
                                                bool is_attribute) {
  Message m;

  for (size_t i = 0; i < str.size(); ++i) {
    const char ch = str[i];
    switch (ch) {
      case '<':
        m << "&lt;";
        break;
      case '>':
        m << "&gt;";
        break;
      case '&':
        m << "&amp;";
        break;
      case '\'':
        if (is_attribute)
          m << "&apos;";
        else
          m << '\'';
        break;
      case '"':
        if (is_attribute)
          m << "&quot;";
        else
          m << '"';
        break;
      default:
        if (IsValidXmlCharacter(static_cast<unsigned char>(ch))) {
          if (is_attribute &&
              IsNormalizableWhitespace(static_cast<unsigned char>(ch)))
            m << "&#x" << String::FormatByte(static_cast<unsigned char>(ch))
              << ";";
          else
            m << ch;
        }
        break;
    }
  }

  return m.GetString();
}

// Returns the given string with all characters invalid in XML removed.
// Currently invalid characters are dropped from the string. An
// alternative is to replace them with certain characters such as . or ?.
std::string XmlUnitTestResultPrinter::RemoveInvalidXmlCharacters(
    const std::string& str) {
  std::string output;
  output.reserve(str.size());
  for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
    if (IsValidXmlCharacter(static_cast<unsigned char>(*it)))
      output.push_back(*it);

  return output;
}

// The following routines generate an XML representation of a UnitTest
// object.
//
// This is how Google Test concepts map to the DTD:
//
// <testsuites name="AllTests">        <-- corresponds to a UnitTest object
//   <testsuite name="testcase-name">  <-- corresponds to a TestSuite object
//     <testcase name="test-name">     <-- corresponds to a TestInfo object
//       <failure message="...">...</failure>
//       <failure message="...">...</failure>
//       <failure message="...">...</failure>
//                                     <-- individual assertion failures
//     </testcase>
//   </testsuite>
// </testsuites>

// Formats the given time in milliseconds as seconds.
std::string FormatTimeInMillisAsSeconds(TimeInMillis ms) {
  ::std::stringstream ss;
  // For the exact N seconds, makes sure output has a trailing decimal point.
  // Sets precision so that we won't have many trailing zeros (e.g., 300 ms
  // will be just 0.3, 410 ms 0.41, and so on)
  ss << std::fixed
     << std::setprecision(
            ms % 1000 == 0 ? 0 : (ms % 100 == 0 ? 1 : (ms % 10 == 0 ? 2 : 3)))
     << std::showpoint;
  ss << (static_cast<double>(ms) * 1e-3);
  return ss.str();
}

static bool PortableLocaltime(time_t seconds, struct tm* out) {
#if defined(_MSC_VER)
  return localtime_s(out, &seconds) == 0;
#elif defined(__MINGW32__) || defined(__MINGW64__)
  // MINGW <time.h> provides neither localtime_r nor localtime_s, but uses
  // Windows' localtime(), which has a thread-local tm buffer.
  struct tm* tm_ptr = localtime(&seconds);  // NOLINT
  if (tm_ptr == nullptr) return false;
  *out = *tm_ptr;
  return true;
#elif defined(__STDC_LIB_EXT1__)
  // Uses localtime_s when available as localtime_r is only available from
  // C23 standard.
  return localtime_s(&seconds, out) != nullptr;
#else
  return localtime_r(&seconds, out) != nullptr;
#endif
}

// Converts the given epoch time in milliseconds to a date string in the ISO
// 8601 format, without the timezone information.
std::string FormatEpochTimeInMillisAsIso8601(TimeInMillis ms) {
  struct tm time_struct;
  if (!PortableLocaltime(static_cast<time_t>(ms / 1000), &time_struct))
    return "";
  // YYYY-MM-DDThh:mm:ss.sss
  return StreamableToString(time_struct.tm_year + 1900) + "-" +
         String::FormatIntWidth2(time_struct.tm_mon + 1) + "-" +
         String::FormatIntWidth2(time_struct.tm_mday) + "T" +
         String::FormatIntWidth2(time_struct.tm_hour) + ":" +
         String::FormatIntWidth2(time_struct.tm_min) + ":" +
         String::FormatIntWidth2(time_struct.tm_sec) + "." +
         String::FormatIntWidthN(static_cast<int>(ms % 1000), 3);
}

// Streams an XML CDATA section, escaping invalid CDATA sequences as needed.
void XmlUnitTestResultPrinter::OutputXmlCDataSection(::std::ostream* stream,
                                                     const char* data) {
  const char* segment = data;
  *stream << "<![CDATA[";
  for (;;) {
    const char* const next_segment = strstr(segment, "]]>");
    if (next_segment != nullptr) {
      stream->write(segment,
                    static_cast<std::streamsize>(next_segment - segment));
      *stream << "]]>]]&gt;<![CDATA[";
      segment = next_segment + strlen("]]>");
    } else {
      *stream << segment;
      break;
    }
  }
  *stream << "]]>";
}

void XmlUnitTestResultPrinter::OutputXmlAttribute(
    std::ostream* stream, const std::string& element_name,
    const std::string& name, const std::string& value) {
  const std::vector<std::string>& allowed_names =
      GetReservedOutputAttributesForElement(element_name);

  GTEST_CHECK_(std::find(allowed_names.begin(), allowed_names.end(), name) !=
               allowed_names.end())
      << "Attribute " << name << " is not allowed for element <" << element_name
      << ">.";

  *stream << " " << name << "=\"" << EscapeXmlAttribute(value) << "\"";
}

// Streams a test suite XML stanza containing the given test result.
void XmlUnitTestResultPrinter::OutputXmlTestSuiteForTestResult(
    ::std::ostream* stream, const TestResult& result) {
  // Output the boilerplate for a minimal test suite with one test.
  *stream << "  <testsuite";
  OutputXmlAttribute(stream, "testsuite", "name", "NonTestSuiteFailure");
  OutputXmlAttribute(stream, "testsuite", "tests", "1");
  OutputXmlAttribute(stream, "testsuite", "failures", "1");
  OutputXmlAttribute(stream, "testsuite", "disabled", "0");
  OutputXmlAttribute(stream, "testsuite", "skipped", "0");
  OutputXmlAttribute(stream, "testsuite", "errors", "0");
  OutputXmlAttribute(stream, "testsuite", "time",
                     FormatTimeInMillisAsSeconds(result.elapsed_time()));
  OutputXmlAttribute(
      stream, "testsuite", "timestamp",
      FormatEpochTimeInMillisAsIso8601(result.start_timestamp()));
  *stream << ">";

  OutputXmlTestCaseForTestResult(stream, result);

  // Complete the test suite.
  *stream << "  </testsuite>\n";
}

// Streams a test case XML stanza containing the given test result.
void XmlUnitTestResultPrinter::OutputXmlTestCaseForTestResult(
    ::std::ostream* stream, const TestResult& result) {
  // Output the boilerplate for a minimal test case with a single test.
  *stream << "    <testcase";
  OutputXmlAttribute(stream, "testcase", "name", "");
  OutputXmlAttribute(stream, "testcase", "status", "run");
  OutputXmlAttribute(stream, "testcase", "result", "completed");
  OutputXmlAttribute(stream, "testcase", "classname", "");
  OutputXmlAttribute(stream, "testcase", "time",
                     FormatTimeInMillisAsSeconds(result.elapsed_time()));
  OutputXmlAttribute(
      stream, "testcase", "timestamp",
      FormatEpochTimeInMillisAsIso8601(result.start_timestamp()));

  // Output the actual test result.
  OutputXmlTestResult(stream, result);
}

// Prints an XML representation of a TestInfo object.
void XmlUnitTestResultPrinter::OutputXmlTestInfo(::std::ostream* stream,
                                                 const char* test_suite_name,
                                                 const TestInfo& test_info) {
  const TestResult& result = *test_info.result();
  const std::string kTestsuite = "testcase";

  if (test_info.is_in_another_shard()) {
    return;
  }

  *stream << "    <testcase";
  OutputXmlAttribute(stream, kTestsuite, "name", test_info.name());

  if (test_info.value_param() != nullptr) {
    OutputXmlAttribute(stream, kTestsuite, "value_param",
                       test_info.value_param());
  }
  if (test_info.type_param() != nullptr) {
    OutputXmlAttribute(stream, kTestsuite, "type_param",
                       test_info.type_param());
  }

  OutputXmlAttribute(stream, kTestsuite, "file", test_info.file());
  OutputXmlAttribute(stream, kTestsuite, "line",
                     StreamableToString(test_info.line()));
  if (GTEST_FLAG_GET(list_tests)) {
    *stream << " />\n";
    return;
  }

  OutputXmlAttribute(stream, kTestsuite, "status",
                     test_info.should_run() ? "run" : "notrun");
  OutputXmlAttribute(stream, kTestsuite, "result",
                     test_info.should_run()
                         ? (result.Skipped() ? "skipped" : "completed")
                         : "suppressed");
  OutputXmlAttribute(stream, kTestsuite, "time",
                     FormatTimeInMillisAsSeconds(result.elapsed_time()));
  OutputXmlAttribute(
      stream, kTestsuite, "timestamp",
      FormatEpochTimeInMillisAsIso8601(result.start_timestamp()));
  OutputXmlAttribute(stream, kTestsuite, "classname", test_suite_name);

  OutputXmlTestResult(stream, result);
}

void XmlUnitTestResultPrinter::OutputXmlTestResult(::std::ostream* stream,
                                                   const TestResult& result) {
  int failures = 0;
  int skips = 0;
  for (int i = 0; i < result.total_part_count(); ++i) {
    const TestPartResult& part = result.GetTestPartResult(i);
    if (part.failed()) {
      if (++failures == 1 && skips == 0) {
        *stream << ">\n";
      }
      const std::string location =
          internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                          part.line_number());
      const std::string summary = location + "\n" + part.summary();
      *stream << "      <failure message=\"" << EscapeXmlAttribute(summary)
              << "\" type=\"\">";
      const std::string detail = location + "\n" + part.message();
      OutputXmlCDataSection(stream, RemoveInvalidXmlCharacters(detail).c_str());
      *stream << "</failure>\n";
    } else if (part.skipped()) {
      if (++skips == 1 && failures == 0) {
        *stream << ">\n";
      }
      const std::string location =
          internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                          part.line_number());
      const std::string summary = location + "\n" + part.summary();
      *stream << "      <skipped message=\""
              << EscapeXmlAttribute(summary.c_str()) << "\">";
      const std::string detail = location + "\n" + part.message();
      OutputXmlCDataSection(stream, RemoveInvalidXmlCharacters(detail).c_str());
      *stream << "</skipped>\n";
    }
  }

  if (failures == 0 && skips == 0 && result.test_property_count() == 0) {
    *stream << " />\n";
  } else {
    if (failures == 0 && skips == 0) {
      *stream << ">\n";
    }
    OutputXmlTestProperties(stream, result, /*indent=*/"      ");
    *stream << "    </testcase>\n";
  }
}

// Prints an XML representation of a TestSuite object
void XmlUnitTestResultPrinter::PrintXmlTestSuite(std::ostream* stream,
                                                 const TestSuite& test_suite) {
  const std::string kTestsuite = "testsuite";
  *stream << "  <" << kTestsuite;
  OutputXmlAttribute(stream, kTestsuite, "name", test_suite.name());
  OutputXmlAttribute(stream, kTestsuite, "tests",
                     StreamableToString(test_suite.reportable_test_count()));
  if (!GTEST_FLAG_GET(list_tests)) {
    OutputXmlAttribute(stream, kTestsuite, "failures",
                       StreamableToString(test_suite.failed_test_count()));
    OutputXmlAttribute(
        stream, kTestsuite, "disabled",
        StreamableToString(test_suite.reportable_disabled_test_count()));
    OutputXmlAttribute(stream, kTestsuite, "skipped",
                       StreamableToString(test_suite.skipped_test_count()));

    OutputXmlAttribute(stream, kTestsuite, "errors", "0");

    OutputXmlAttribute(stream, kTestsuite, "time",
                       FormatTimeInMillisAsSeconds(test_suite.elapsed_time()));
    OutputXmlAttribute(
        stream, kTestsuite, "timestamp",
        FormatEpochTimeInMillisAsIso8601(test_suite.start_timestamp()));
  }
  *stream << ">\n";
  OutputXmlTestProperties(stream, test_suite.ad_hoc_test_result(),
                          /*indent=*/"    ");
  for (int i = 0; i < test_suite.total_test_count(); ++i) {
    if (test_suite.GetTestInfo(i)->is_reportable())
      OutputXmlTestInfo(stream, test_suite.name(), *test_suite.GetTestInfo(i));
  }
  if (test_suite.ad_hoc_test_result().Failed()) {
    OutputXmlTestCaseForTestResult(stream, test_suite.ad_hoc_test_result());
  }

  *stream << "  </" << kTestsuite << ">\n";
}

// Prints an XML summary of unit_test to output stream out.
void XmlUnitTestResultPrinter::PrintXmlUnitTest(std::ostream* stream,
                                                const UnitTest& unit_test) {
  const std::string kTestsuites = "testsuites";

  *stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  *stream << "<" << kTestsuites;

  OutputXmlAttribute(stream, kTestsuites, "tests",
                     StreamableToString(unit_test.reportable_test_count()));
  OutputXmlAttribute(stream, kTestsuites, "failures",
                     StreamableToString(unit_test.failed_test_count()));
  OutputXmlAttribute(
      stream, kTestsuites, "disabled",
      StreamableToString(unit_test.reportable_disabled_test_count()));
  OutputXmlAttribute(stream, kTestsuites, "errors", "0");
  OutputXmlAttribute(stream, kTestsuites, "time",
                     FormatTimeInMillisAsSeconds(unit_test.elapsed_time()));
  OutputXmlAttribute(
      stream, kTestsuites, "timestamp",
      FormatEpochTimeInMillisAsIso8601(unit_test.start_timestamp()));

  if (GTEST_FLAG_GET(shuffle)) {
    OutputXmlAttribute(stream, kTestsuites, "random_seed",
                       StreamableToString(unit_test.random_seed()));
  }

  OutputXmlAttribute(stream, kTestsuites, "name", "AllTests");
  *stream << ">\n";

  OutputXmlTestProperties(stream, unit_test.ad_hoc_test_result(),
                          /*indent=*/"  ");
  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    if (unit_test.GetTestSuite(i)->reportable_test_count() > 0)
      PrintXmlTestSuite(stream, *unit_test.GetTestSuite(i));
  }

  // If there was a test failure outside of one of the test suites (like in a
  // test environment) include that in the output.
  if (unit_test.ad_hoc_test_result().Failed()) {
    OutputXmlTestSuiteForTestResult(stream, unit_test.ad_hoc_test_result());
  }

  *stream << "</" << kTestsuites << ">\n";
}

void XmlUnitTestResultPrinter::PrintXmlTestsList(
    std::ostream* stream, const std::vector<TestSuite*>& test_suites) {
  const std::string kTestsuites = "testsuites";

  *stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  *stream << "<" << kTestsuites;

  int total_tests = 0;
  for (auto test_suite : test_suites) {
    total_tests += test_suite->total_test_count();
  }
  OutputXmlAttribute(stream, kTestsuites, "tests",
                     StreamableToString(total_tests));
  OutputXmlAttribute(stream, kTestsuites, "name", "AllTests");
  *stream << ">\n";

  for (auto test_suite : test_suites) {
    PrintXmlTestSuite(stream, *test_suite);
  }
  *stream << "</" << kTestsuites << ">\n";
}

void XmlUnitTestResultPrinter::OutputXmlTestProperties(
    std::ostream* stream, const TestResult& result, const std::string& indent) {
  const std::string kProperties = "properties";
  const std::string kProperty = "property";

  if (result.test_property_count() <= 0) {
    return;
  }

  *stream << indent << "<" << kProperties << ">\n";
  for (int i = 0; i < result.test_property_count(); ++i) {
    const TestProperty& property = result.GetTestProperty(i);
    *stream << indent << "  <" << kProperty;
    *stream << " name=\"" << EscapeXmlAttribute(property.key()) << "\"";
    *stream << " value=\"" << EscapeXmlAttribute(property.value()) << "\"";
    *stream << "/>\n";
  }
  *stream << indent << "</" << kProperties << ">\n";
}

// End XmlUnitTestResultPrinter
#endif  // GTEST_HAS_FILE_SYSTEM

#if GTEST_HAS_FILE_SYSTEM
// This class generates an JSON output file.
class JsonUnitTestResultPrinter : public EmptyTestEventListener {
 public:
  explicit JsonUnitTestResultPrinter(const char* output_file);

  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;

  // Prints an JSON summary of all unit tests.
  static void PrintJsonTestList(::std::ostream* stream,
                                const std::vector<TestSuite*>& test_suites);

 private:
  // Returns an JSON-escaped copy of the input string str.
  static std::string EscapeJson(const std::string& str);

  //// Verifies that the given attribute belongs to the given element and
  //// streams the attribute as JSON.
  static void OutputJsonKey(std::ostream* stream,
                            const std::string& element_name,
                            const std::string& name, const std::string& value,
                            const std::string& indent, bool comma = true);
  static void OutputJsonKey(std::ostream* stream,
                            const std::string& element_name,
                            const std::string& name, int value,
                            const std::string& indent, bool comma = true);

  // Streams a test suite JSON stanza containing the given test result.
  //
  // Requires: result.Failed()
  static void OutputJsonTestSuiteForTestResult(::std::ostream* stream,
                                               const TestResult& result);

  // Streams a test case JSON stanza containing the given test result.
  //
  // Requires: result.Failed()
  static void OutputJsonTestCaseForTestResult(::std::ostream* stream,
                                              const TestResult& result);

  // Streams a JSON representation of a TestResult object.
  static void OutputJsonTestResult(::std::ostream* stream,
                                   const TestResult& result);

  // Streams a JSON representation of a TestInfo object.
  static void OutputJsonTestInfo(::std::ostream* stream,
                                 const char* test_suite_name,
                                 const TestInfo& test_info);

  // Prints a JSON representation of a TestSuite object
  static void PrintJsonTestSuite(::std::ostream* stream,
                                 const TestSuite& test_suite);

  // Prints a JSON summary of unit_test to output stream out.
  static void PrintJsonUnitTest(::std::ostream* stream,
                                const UnitTest& unit_test);

  // Produces a string representing the test properties in a result as
  // a JSON dictionary.
  static std::string TestPropertiesAsJson(const TestResult& result,
                                          const std::string& indent);

  // The output file.
  const std::string output_file_;

  JsonUnitTestResultPrinter(const JsonUnitTestResultPrinter&) = delete;
  JsonUnitTestResultPrinter& operator=(const JsonUnitTestResultPrinter&) =
      delete;
};

// Creates a new JsonUnitTestResultPrinter.
JsonUnitTestResultPrinter::JsonUnitTestResultPrinter(const char* output_file)
    : output_file_(output_file) {
  if (output_file_.empty()) {
    GTEST_LOG_(FATAL) << "JSON output file may not be null";
  }
}

void JsonUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                   int /*iteration*/) {
  FILE* jsonout = OpenFileForWriting(output_file_);
  std::stringstream stream;
  PrintJsonUnitTest(&stream, unit_test);
  fprintf(jsonout, "%s", StringStreamToString(&stream).c_str());
  fclose(jsonout);
}

// Returns an JSON-escaped copy of the input string str.
std::string JsonUnitTestResultPrinter::EscapeJson(const std::string& str) {
  Message m;

  for (size_t i = 0; i < str.size(); ++i) {
    const char ch = str[i];
    switch (ch) {
      case '\\':
      case '"':
      case '/':
        m << '\\' << ch;
        break;
      case '\b':
        m << "\\b";
        break;
      case '\t':
        m << "\\t";
        break;
      case '\n':
        m << "\\n";
        break;
      case '\f':
        m << "\\f";
        break;
      case '\r':
        m << "\\r";
        break;
      default:
        if (ch < ' ') {
          m << "\\u00" << String::FormatByte(static_cast<unsigned char>(ch));
        } else {
          m << ch;
        }
        break;
    }
  }

  return m.GetString();
}

// The following routines generate an JSON representation of a UnitTest
// object.

// Formats the given time in milliseconds as seconds.
static std::string FormatTimeInMillisAsDuration(TimeInMillis ms) {
  ::std::stringstream ss;
  ss << (static_cast<double>(ms) * 1e-3) << "s";
  return ss.str();
}

// Converts the given epoch time in milliseconds to a date string in the
// RFC3339 format, without the timezone information.
static std::string FormatEpochTimeInMillisAsRFC3339(TimeInMillis ms) {
  struct tm time_struct;
  if (!PortableLocaltime(static_cast<time_t>(ms / 1000), &time_struct))
    return "";
  // YYYY-MM-DDThh:mm:ss
  return StreamableToString(time_struct.tm_year + 1900) + "-" +
         String::FormatIntWidth2(time_struct.tm_mon + 1) + "-" +
         String::FormatIntWidth2(time_struct.tm_mday) + "T" +
         String::FormatIntWidth2(time_struct.tm_hour) + ":" +
         String::FormatIntWidth2(time_struct.tm_min) + ":" +
         String::FormatIntWidth2(time_struct.tm_sec) + "Z";
}

static inline std::string Indent(size_t width) {
  return std::string(width, ' ');
}

void JsonUnitTestResultPrinter::OutputJsonKey(std::ostream* stream,
                                              const std::string& element_name,
                                              const std::string& name,
                                              const std::string& value,
                                              const std::string& indent,
                                              bool comma) {
  const std::vector<std::string>& allowed_names =
      GetReservedOutputAttributesForElement(element_name);

  GTEST_CHECK_(std::find(allowed_names.begin(), allowed_names.end(), name) !=
               allowed_names.end())
      << "Key \"" << name << "\" is not allowed for value \"" << element_name
      << "\".";

  *stream << indent << "\"" << name << "\": \"" << EscapeJson(value) << "\"";
  if (comma) *stream << ",\n";
}

void JsonUnitTestResultPrinter::OutputJsonKey(
    std::ostream* stream, const std::string& element_name,
    const std::string& name, int value, const std::string& indent, bool comma) {
  const std::vector<std::string>& allowed_names =
      GetReservedOutputAttributesForElement(element_name);

  GTEST_CHECK_(std::find(allowed_names.begin(), allowed_names.end(), name) !=
               allowed_names.end())
      << "Key \"" << name << "\" is not allowed for value \"" << element_name
      << "\".";

  *stream << indent << "\"" << name << "\": " << StreamableToString(value);
  if (comma) *stream << ",\n";
}

// Streams a test suite JSON stanza containing the given test result.
void JsonUnitTestResultPrinter::OutputJsonTestSuiteForTestResult(
    ::std::ostream* stream, const TestResult& result) {
  // Output the boilerplate for a new test suite.
  *stream << Indent(4) << "{\n";
  OutputJsonKey(stream, "testsuite", "name", "NonTestSuiteFailure", Indent(6));
  OutputJsonKey(stream, "testsuite", "tests", 1, Indent(6));
  if (!GTEST_FLAG_GET(list_tests)) {
    OutputJsonKey(stream, "testsuite", "failures", 1, Indent(6));
    OutputJsonKey(stream, "testsuite", "disabled", 0, Indent(6));
    OutputJsonKey(stream, "testsuite", "skipped", 0, Indent(6));
    OutputJsonKey(stream, "testsuite", "errors", 0, Indent(6));
    OutputJsonKey(stream, "testsuite", "time",
                  FormatTimeInMillisAsDuration(result.elapsed_time()),
                  Indent(6));
    OutputJsonKey(stream, "testsuite", "timestamp",
                  FormatEpochTimeInMillisAsRFC3339(result.start_timestamp()),
                  Indent(6));
  }
  *stream << Indent(6) << "\"testsuite\": [\n";

  OutputJsonTestCaseForTestResult(stream, result);

  // Finish the test suite.
  *stream << "\n" << Indent(6) << "]\n" << Indent(4) << "}";
}

// Streams a test case JSON stanza containing the given test result.
void JsonUnitTestResultPrinter::OutputJsonTestCaseForTestResult(
    ::std::ostream* stream, const TestResult& result) {
  // Output the boilerplate for a new test case.
  *stream << Indent(8) << "{\n";
  OutputJsonKey(stream, "testcase", "name", "", Indent(10));
  OutputJsonKey(stream, "testcase", "status", "RUN", Indent(10));
  OutputJsonKey(stream, "testcase", "result", "COMPLETED", Indent(10));
  OutputJsonKey(stream, "testcase", "timestamp",
                FormatEpochTimeInMillisAsRFC3339(result.start_timestamp()),
                Indent(10));
  OutputJsonKey(stream, "testcase", "time",
                FormatTimeInMillisAsDuration(result.elapsed_time()),
                Indent(10));
  OutputJsonKey(stream, "testcase", "classname", "", Indent(10), false);
  *stream << TestPropertiesAsJson(result, Indent(10));

  // Output the actual test result.
  OutputJsonTestResult(stream, result);
}

// Prints a JSON representation of a TestInfo object.
void JsonUnitTestResultPrinter::OutputJsonTestInfo(::std::ostream* stream,
                                                   const char* test_suite_name,
                                                   const TestInfo& test_info) {
  const TestResult& result = *test_info.result();
  const std::string kTestsuite = "testcase";
  const std::string kIndent = Indent(10);

  *stream << Indent(8) << "{\n";
  OutputJsonKey(stream, kTestsuite, "name", test_info.name(), kIndent);

  if (test_info.value_param() != nullptr) {
    OutputJsonKey(stream, kTestsuite, "value_param", test_info.value_param(),
                  kIndent);
  }
  if (test_info.type_param() != nullptr) {
    OutputJsonKey(stream, kTestsuite, "type_param", test_info.type_param(),
                  kIndent);
  }

  OutputJsonKey(stream, kTestsuite, "file", test_info.file(), kIndent);
  OutputJsonKey(stream, kTestsuite, "line", test_info.line(), kIndent, false);
  if (GTEST_FLAG_GET(list_tests)) {
    *stream << "\n" << Indent(8) << "}";
    return;
  } else {
    *stream << ",\n";
  }

  OutputJsonKey(stream, kTestsuite, "status",
                test_info.should_run() ? "RUN" : "NOTRUN", kIndent);
  OutputJsonKey(stream, kTestsuite, "result",
                test_info.should_run()
                    ? (result.Skipped() ? "SKIPPED" : "COMPLETED")
                    : "SUPPRESSED",
                kIndent);
  OutputJsonKey(stream, kTestsuite, "timestamp",
                FormatEpochTimeInMillisAsRFC3339(result.start_timestamp()),
                kIndent);
  OutputJsonKey(stream, kTestsuite, "time",
                FormatTimeInMillisAsDuration(result.elapsed_time()), kIndent);
  OutputJsonKey(stream, kTestsuite, "classname", test_suite_name, kIndent,
                false);
  *stream << TestPropertiesAsJson(result, kIndent);

  OutputJsonTestResult(stream, result);
}

void JsonUnitTestResultPrinter::OutputJsonTestResult(::std::ostream* stream,
                                                     const TestResult& result) {
  const std::string kIndent = Indent(10);

  {
    int failures = 0;
    for (int i = 0; i < result.total_part_count(); ++i) {
      const TestPartResult& part = result.GetTestPartResult(i);
      if (part.failed()) {
        *stream << ",\n";
        if (++failures == 1) {
          *stream << kIndent << "\"" << "failures" << "\": [\n";
        }
        const std::string location =
            internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                            part.line_number());
        const std::string message =
            EscapeJson(location + "\n" + part.message());
        *stream << kIndent << "  {\n"
                << kIndent << "    \"failure\": \"" << message << "\",\n"
                << kIndent << "    \"type\": \"\"\n"
                << kIndent << "  }";
      }
    }

    if (failures > 0) *stream << "\n" << kIndent << "]";
  }

  {
    int skipped = 0;
    for (int i = 0; i < result.total_part_count(); ++i) {
      const TestPartResult& part = result.GetTestPartResult(i);
      if (part.skipped()) {
        *stream << ",\n";
        if (++skipped == 1) {
          *stream << kIndent << "\"" << "skipped" << "\": [\n";
        }
        const std::string location =
            internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                            part.line_number());
        const std::string message =
            EscapeJson(location + "\n" + part.message());
        *stream << kIndent << "  {\n"
                << kIndent << "    \"message\": \"" << message << "\"\n"
                << kIndent << "  }";
      }
    }

    if (skipped > 0) *stream << "\n" << kIndent << "]";
  }

  *stream << "\n" << Indent(8) << "}";
}

// Prints an JSON representation of a TestSuite object
void JsonUnitTestResultPrinter::PrintJsonTestSuite(
    std::ostream* stream, const TestSuite& test_suite) {
  const std::string kTestsuite = "testsuite";
  const std::string kIndent = Indent(6);

  *stream << Indent(4) << "{\n";
  OutputJsonKey(stream, kTestsuite, "name", test_suite.name(), kIndent);
  OutputJsonKey(stream, kTestsuite, "tests", test_suite.reportable_test_count(),
                kIndent);
  if (!GTEST_FLAG_GET(list_tests)) {
    OutputJsonKey(stream, kTestsuite, "failures",
                  test_suite.failed_test_count(), kIndent);
    OutputJsonKey(stream, kTestsuite, "disabled",
                  test_suite.reportable_disabled_test_count(), kIndent);
    OutputJsonKey(stream, kTestsuite, "errors", 0, kIndent);
    OutputJsonKey(
        stream, kTestsuite, "timestamp",
        FormatEpochTimeInMillisAsRFC3339(test_suite.start_timestamp()),
        kIndent);
    OutputJsonKey(stream, kTestsuite, "time",
                  FormatTimeInMillisAsDuration(test_suite.elapsed_time()),
                  kIndent, false);
    *stream << TestPropertiesAsJson(test_suite.ad_hoc_test_result(), kIndent)
            << ",\n";
  }

  *stream << kIndent << "\"" << kTestsuite << "\": [\n";

  bool comma = false;
  for (int i = 0; i < test_suite.total_test_count(); ++i) {
    if (test_suite.GetTestInfo(i)->is_reportable()) {
      if (comma) {
        *stream << ",\n";
      } else {
        comma = true;
      }
      OutputJsonTestInfo(stream, test_suite.name(), *test_suite.GetTestInfo(i));
    }
  }

  // If there was a failure in the test suite setup or teardown include that in
  // the output.
  if (test_suite.ad_hoc_test_result().Failed()) {
    if (comma) {
      *stream << ",\n";
    }
    OutputJsonTestCaseForTestResult(stream, test_suite.ad_hoc_test_result());
  }

  *stream << "\n" << kIndent << "]\n" << Indent(4) << "}";
}

// Prints a JSON summary of unit_test to output stream out.
void JsonUnitTestResultPrinter::PrintJsonUnitTest(std::ostream* stream,
                                                  const UnitTest& unit_test) {
  const std::string kTestsuites = "testsuites";
  const std::string kIndent = Indent(2);
  *stream << "{\n";

  OutputJsonKey(stream, kTestsuites, "tests", unit_test.reportable_test_count(),
                kIndent);
  OutputJsonKey(stream, kTestsuites, "failures", unit_test.failed_test_count(),
                kIndent);
  OutputJsonKey(stream, kTestsuites, "disabled",
                unit_test.reportable_disabled_test_count(), kIndent);
  OutputJsonKey(stream, kTestsuites, "errors", 0, kIndent);
  if (GTEST_FLAG_GET(shuffle)) {
    OutputJsonKey(stream, kTestsuites, "random_seed", unit_test.random_seed(),
                  kIndent);
  }
  OutputJsonKey(stream, kTestsuites, "timestamp",
                FormatEpochTimeInMillisAsRFC3339(unit_test.start_timestamp()),
                kIndent);
  OutputJsonKey(stream, kTestsuites, "time",
                FormatTimeInMillisAsDuration(unit_test.elapsed_time()), kIndent,
                false);

  *stream << TestPropertiesAsJson(unit_test.ad_hoc_test_result(), kIndent)
          << ",\n";

  OutputJsonKey(stream, kTestsuites, "name", "AllTests", kIndent);
  *stream << kIndent << "\"" << kTestsuites << "\": [\n";

  bool comma = false;
  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    if (unit_test.GetTestSuite(i)->reportable_test_count() > 0) {
      if (comma) {
        *stream << ",\n";
      } else {
        comma = true;
      }
      PrintJsonTestSuite(stream, *unit_test.GetTestSuite(i));
    }
  }

  // If there was a test failure outside of one of the test suites (like in a
  // test environment) include that in the output.
  if (unit_test.ad_hoc_test_result().Failed()) {
    if (comma) {
      *stream << ",\n";
    }
    OutputJsonTestSuiteForTestResult(stream, unit_test.ad_hoc_test_result());
  }

  *stream << "\n"
          << kIndent << "]\n"
          << "}\n";
}

void JsonUnitTestResultPrinter::PrintJsonTestList(
    std::ostream* stream, const std::vector<TestSuite*>& test_suites) {
  const std::string kTestsuites = "testsuites";
  const std::string kIndent = Indent(2);
  *stream << "{\n";
  int total_tests = 0;
  for (auto test_suite : test_suites) {
    total_tests += test_suite->total_test_count();
  }
  OutputJsonKey(stream, kTestsuites, "tests", total_tests, kIndent);

  OutputJsonKey(stream, kTestsuites, "name", "AllTests", kIndent);
  *stream << kIndent << "\"" << kTestsuites << "\": [\n";

  for (size_t i = 0; i < test_suites.size(); ++i) {
    if (i != 0) {
      *stream << ",\n";
    }
    PrintJsonTestSuite(stream, *test_suites[i]);
  }

  *stream << "\n"
          << kIndent << "]\n"
          << "}\n";
}
// Produces a string representing the test properties in a result as
// a JSON dictionary.
std::string JsonUnitTestResultPrinter::TestPropertiesAsJson(
    const TestResult& result, const std::string& indent) {
  Message attributes;
  for (int i = 0; i < result.test_property_count(); ++i) {
    const TestProperty& property = result.GetTestProperty(i);
    attributes << ",\n"
               << indent << "\"" << property.key() << "\": " << "\""
               << EscapeJson(property.value()) << "\"";
  }
  return attributes.GetString();
}

// End JsonUnitTestResultPrinter
#endif  // GTEST_HAS_FILE_SYSTEM

#if GTEST_CAN_STREAM_RESULTS_

// Checks if str contains '=', '&', '%' or '\n' characters. If yes,
// replaces them by "%xx" where xx is their hexadecimal value. For
// example, replaces "=" with "%3D".  This algorithm is O(strlen(str))
// in both time and space -- important as the input str may contain an
// arbitrarily long test failure message and stack trace.
std::string StreamingListener::UrlEncode(const char* str) {
  std::string result;
  result.reserve(strlen(str) + 1);
  for (char ch = *str; ch != '\0'; ch = *++str) {
    switch (ch) {
      case '%':
      case '=':
      case '&':
      case '\n':
        result.push_back('%');
        result.append(String::FormatByte(static_cast<unsigned char>(ch)));
        break;
      default:
        result.push_back(ch);
        break;
    }
  }
  return result;
}

void StreamingListener::SocketWriter::MakeConnection() {
  GTEST_CHECK_(sockfd_ == -1)
      << "MakeConnection() can't be called when there is already a connection.";

  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;  // To allow both IPv4 and IPv6 addresses.
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* servinfo = nullptr;

  // Use the getaddrinfo() to get a linked list of IP addresses for
  // the given host name.
  const int error_num =
      getaddrinfo(host_name_.c_str(), port_num_.c_str(), &hints, &servinfo);
  if (error_num != 0) {
    GTEST_LOG_(WARNING) << "stream_result_to: getaddrinfo() failed: "
                        << gai_strerror(error_num);
  }

  // Loop through all the results and connect to the first we can.
  for (addrinfo* cur_addr = servinfo; sockfd_ == -1 && cur_addr != nullptr;
       cur_addr = cur_addr->ai_next) {
    sockfd_ = socket(cur_addr->ai_family, cur_addr->ai_socktype,
                     cur_addr->ai_protocol);
    if (sockfd_ != -1) {
      // Connect the client socket to the server socket.
      if (connect(sockfd_, cur_addr->ai_addr, cur_addr->ai_addrlen) == -1) {
        close(sockfd_);
        sockfd_ = -1;
      }
    }
  }

  freeaddrinfo(servinfo);  // all done with this structure

  if (sockfd_ == -1) {
    GTEST_LOG_(WARNING) << "stream_result_to: failed to connect to "
                        << host_name_ << ":" << port_num_;
  }
}

// End of class Streaming Listener
#endif  // GTEST_CAN_STREAM_RESULTS__

// class OsStackTraceGetter

const char* const OsStackTraceGetterInterface::kElidedFramesMarker =
    "... " GTEST_NAME_ " internal frames ...";

std::string OsStackTraceGetter::CurrentStackTrace(int max_depth, int skip_count)
    GTEST_LOCK_EXCLUDED_(mutex_) {
#ifdef GTEST_HAS_ABSL
  std::string result;

  if (max_depth <= 0) {
    return result;
  }

  max_depth = std::min(max_depth, kMaxStackTraceDepth);

  std::vector<void*> raw_stack(max_depth);
  // Skips the frames requested by the caller, plus this function.
  const int raw_stack_size =
      absl::GetStackTrace(&raw_stack[0], max_depth, skip_count + 1);

  void* caller_frame = nullptr;
  {
    MutexLock lock(&mutex_);
    caller_frame = caller_frame_;
  }

  for (int i = 0; i < raw_stack_size; ++i) {
    if (raw_stack[i] == caller_frame &&
        !GTEST_FLAG_GET(show_internal_stack_frames)) {
      // Add a marker to the trace and stop adding frames.
      absl::StrAppend(&result, kElidedFramesMarker, "\n");
      break;
    }

    char tmp[1024];
    const char* symbol = "(unknown)";
    if (absl::Symbolize(raw_stack[i], tmp, sizeof(tmp))) {
      symbol = tmp;
    }

    char line[1024];
    snprintf(line, sizeof(line), "  %p: %s\n", raw_stack[i], symbol);
    result += line;
  }

  return result;

#else   // !GTEST_HAS_ABSL
  static_cast<void>(max_depth);
  static_cast<void>(skip_count);
  return "";
#endif  // GTEST_HAS_ABSL
}

void OsStackTraceGetter::UponLeavingGTest() GTEST_LOCK_EXCLUDED_(mutex_) {
#ifdef GTEST_HAS_ABSL
  void* caller_frame = nullptr;
  if (absl::GetStackTrace(&caller_frame, 1, 3) <= 0) {
    caller_frame = nullptr;
  }

  MutexLock lock(&mutex_);
  caller_frame_ = caller_frame;
#endif  // GTEST_HAS_ABSL
}

#ifdef GTEST_HAS_DEATH_TEST
// A helper class that creates the premature-exit file in its
// constructor and deletes the file in its destructor.
class ScopedPrematureExitFile {
 public:
  explicit ScopedPrematureExitFile(const char* premature_exit_filepath)
      : premature_exit_filepath_(
            premature_exit_filepath ? premature_exit_filepath : "") {
    // If a path to the premature-exit file is specified...
    if (!premature_exit_filepath_.empty()) {
      // create the file with a single "0" character in it.  I/O
      // errors are ignored as there's nothing better we can do and we
      // don't want to fail the test because of this.
      FILE* pfile = posix::FOpen(premature_exit_filepath_.c_str(), "w");
      fwrite("0", 1, 1, pfile);
      fclose(pfile);
    }
  }

  ~ScopedPrematureExitFile() {
#ifndef GTEST_OS_ESP8266
    if (!premature_exit_filepath_.empty()) {
      int retval = remove(premature_exit_filepath_.c_str());
      if (retval) {
        GTEST_LOG_(ERROR) << "Failed to remove premature exit filepath \""
                          << premature_exit_filepath_ << "\" with error "
                          << retval;
      }
    }
#endif
  }

 private:
  const std::string premature_exit_filepath_;

  ScopedPrematureExitFile(const ScopedPrematureExitFile&) = delete;
  ScopedPrematureExitFile& operator=(const ScopedPrematureExitFile&) = delete;
};
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace internal

// class TestEventListeners

TestEventListeners::TestEventListeners()
    : repeater_(new internal::TestEventRepeater()),
      default_result_printer_(nullptr),
      default_xml_generator_(nullptr) {}

TestEventListeners::~TestEventListeners() { delete repeater_; }

// Returns the standard listener responsible for the default console
// output.  Can be removed from the listeners list to shut down default
// console output.  Note that removing this object from the listener list
// with Release transfers its ownership to the user.
void TestEventListeners::Append(TestEventListener* listener) {
  repeater_->Append(listener);
}

// Removes the given event listener from the list and returns it.  It then
// becomes the caller's responsibility to delete the listener. Returns
// NULL if the listener is not found in the list.
TestEventListener* TestEventListeners::Release(TestEventListener* listener) {
  if (listener == default_result_printer_)
    default_result_printer_ = nullptr;
  else if (listener == default_xml_generator_)
    default_xml_generator_ = nullptr;
  return repeater_->Release(listener);
}

// Returns repeater that broadcasts the TestEventListener events to all
// subscribers.
TestEventListener* TestEventListeners::repeater() { return repeater_; }

// Sets the default_result_printer attribute to the provided listener.
// The listener is also added to the listener list and previous
// default_result_printer is removed from it and deleted. The listener can
// also be NULL in which case it will not be added to the list. Does
// nothing if the previous and the current listener objects are the same.
void TestEventListeners::SetDefaultResultPrinter(TestEventListener* listener) {
  if (default_result_printer_ != listener) {
    // It is an error to pass this method a listener that is already in the
    // list.
    delete Release(default_result_printer_);
    default_result_printer_ = listener;
    if (listener != nullptr) Append(listener);
  }
}

// Sets the default_xml_generator attribute to the provided listener.  The
// listener is also added to the listener list and previous
// default_xml_generator is removed from it and deleted. The listener can
// also be NULL in which case it will not be added to the list. Does
// nothing if the previous and the current listener objects are the same.
void TestEventListeners::SetDefaultXmlGenerator(TestEventListener* listener) {
  if (default_xml_generator_ != listener) {
    // It is an error to pass this method a listener that is already in the
    // list.
    delete Release(default_xml_generator_);
    default_xml_generator_ = listener;
    if (listener != nullptr) Append(listener);
  }
}

// Controls whether events will be forwarded by the repeater to the
// listeners in the list.
bool TestEventListeners::EventForwardingEnabled() const {
  return repeater_->forwarding_enabled();
}

void TestEventListeners::SuppressEventForwarding(bool suppress) {
  repeater_->set_forwarding_enabled(!suppress);
}

// class UnitTest

// Gets the singleton UnitTest object.  The first time this method is
// called, a UnitTest object is constructed and returned.  Consecutive
// calls will return the same object.
//
// We don't protect this under mutex_ as a user is not supposed to
// call this before main() starts, from which point on the return
// value will never change.
UnitTest* UnitTest::GetInstance() {
  // CodeGear C++Builder insists on a public destructor for the
  // default implementation.  Use this implementation to keep good OO
  // design with private destructor.

#if defined(__BORLANDC__)
  static UnitTest* const instance = new UnitTest;
  return instance;
#else
  static UnitTest instance;
  return &instance;
#endif  // defined(__BORLANDC__)
}

// Gets the number of successful test suites.
int UnitTest::successful_test_suite_count() const {
  return impl()->successful_test_suite_count();
}

// Gets the number of failed test suites.
int UnitTest::failed_test_suite_count() const {
  return impl()->failed_test_suite_count();
}

// Gets the number of all test suites.
int UnitTest::total_test_suite_count() const {
  return impl()->total_test_suite_count();
}

// Gets the number of all test suites that contain at least one test
// that should run.
int UnitTest::test_suite_to_run_count() const {
  return impl()->test_suite_to_run_count();
}

//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
int UnitTest::successful_test_case_count() const {
  return impl()->successful_test_suite_count();
}
int UnitTest::failed_test_case_count() const {
  return impl()->failed_test_suite_count();
}
int UnitTest::total_test_case_count() const {
  return impl()->total_test_suite_count();
}
int UnitTest::test_case_to_run_count() const {
  return impl()->test_suite_to_run_count();
}
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_

// Gets the number of successful tests.
int UnitTest::successful_test_count() const {
  return impl()->successful_test_count();
}

// Gets the number of skipped tests.
int UnitTest::skipped_test_count() const {
  return impl()->skipped_test_count();
}

// Gets the number of failed tests.
int UnitTest::failed_test_count() const { return impl()->failed_test_count(); }

// Gets the number of disabled tests that will be reported in the XML report.
int UnitTest::reportable_disabled_test_count() const {
  return impl()->reportable_disabled_test_count();
}

// Gets the number of disabled tests.
int UnitTest::disabled_test_count() const {
  return impl()->disabled_test_count();
}

// Gets the number of tests to be printed in the XML report.
int UnitTest::reportable_test_count() const {
  return impl()->reportable_test_count();
}

// Gets the number of all tests.
int UnitTest::total_test_count() const { return impl()->total_test_count(); }

// Gets the number of tests that should run.
int UnitTest::test_to_run_count() const { return impl()->test_to_run_count(); }

// Gets the time of the test program start, in ms from the start of the
// UNIX epoch.
internal::TimeInMillis UnitTest::start_timestamp() const {
  return impl()->start_timestamp();
}

// Gets the elapsed time, in milliseconds.
internal::TimeInMillis UnitTest::elapsed_time() const {
  return impl()->elapsed_time();
}

// Returns true if and only if the unit test passed (i.e. all test suites
// passed).
bool UnitTest::Passed() const { return impl()->Passed(); }

// Returns true if and only if the unit test failed (i.e. some test suite
// failed or something outside of all tests failed).
bool UnitTest::Failed() const { return impl()->Failed(); }

// Gets the i-th test suite among all the test suites. i can range from 0 to
// total_test_suite_count() - 1. If i is not in that range, returns NULL.
const TestSuite* UnitTest::GetTestSuite(int i) const {
  return impl()->GetTestSuite(i);
}

//  Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
const TestCase* UnitTest::GetTestCase(int i) const {
  return impl()->GetTestCase(i);
}
#endif  //  GTEST_REMOVE_LEGACY_TEST_CASEAPI_

// Returns the TestResult containing information on test failures and
// properties logged outside of individual test suites.
const TestResult& UnitTest::ad_hoc_test_result() const {
  return *impl()->ad_hoc_test_result();
}

// Gets the i-th test suite among all the test suites. i can range from 0 to
// total_test_suite_count() - 1. If i is not in that range, returns NULL.
TestSuite* UnitTest::GetMutableTestSuite(int i) {
  return impl()->GetMutableSuiteCase(i);
}

void UnitTest::UponLeavingGTest() {
  impl()->os_stack_trace_getter()->UponLeavingGTest();
}

// Sets the TestSuite object for the test that's currently running.
void UnitTest::set_current_test_suite(TestSuite* a_current_test_suite) {
  internal::MutexLock lock(&mutex_);
  impl_->set_current_test_suite(a_current_test_suite);
}

// Sets the TestInfo object for the test that's currently running.
void UnitTest::set_current_test_info(TestInfo* a_current_test_info) {
  internal::MutexLock lock(&mutex_);
  impl_->set_current_test_info(a_current_test_info);
}

// Returns the list of event listeners that can be used to track events
// inside Google Test.
TestEventListeners& UnitTest::listeners() { return *impl()->listeners(); }

// Registers and returns a global test environment.  When a test
// program is run, all global test environments will be set-up in the
// order they were registered.  After all tests in the program have
// finished, all global test environments will be torn-down in the
// *reverse* order they were registered.
//
// The UnitTest object takes ownership of the given environment.
//
// We don't protect this under mutex_, as we only support calling it
// from the main thread.
Environment* UnitTest::AddEnvironment(Environment* env) {
  if (env == nullptr) {
    return nullptr;
  }

  impl_->environments().push_back(env);
  return env;
}

// Adds a TestPartResult to the current TestResult object.  All Google Test
// assertion macros (e.g. ASSERT_TRUE, EXPECT_EQ, etc) eventually call
// this to report their results.  The user code should use the
// assertion macros instead of calling this directly.
void UnitTest::AddTestPartResult(TestPartResult::Type result_type,
                                 const char* file_name, int line_number,
                                 const std::string& message,
                                 const std::string& os_stack_trace)
    GTEST_LOCK_EXCLUDED_(mutex_) {
  Message msg;
  msg << message;

  internal::MutexLock lock(&mutex_);
  if (!impl_->gtest_trace_stack().empty()) {
    msg << "\n" << GTEST_NAME_ << " trace:";

    for (size_t i = impl_->gtest_trace_stack().size(); i > 0; --i) {
      const internal::TraceInfo& trace = impl_->gtest_trace_stack()[i - 1];
      msg << "\n"
          << internal::FormatFileLocation(trace.file, trace.line) << " "
          << trace.message;
    }
  }

  if (os_stack_trace.c_str() != nullptr && !os_stack_trace.empty()) {
    msg << internal::kStackTraceMarker << os_stack_trace;
  } else {
    msg << "\n";
  }

  const TestPartResult result = TestPartResult(
      result_type, file_name, line_number, msg.GetString().c_str());
  impl_->GetTestPartResultReporterForCurrentThread()->ReportTestPartResult(
      result);

  if (result_type != TestPartResult::kSuccess &&
      result_type != TestPartResult::kSkip) {
    // gtest_break_on_failure takes precedence over
    // gtest_throw_on_failure.  This allows a user to set the latter
    // in the code (perhaps in order to use Google Test assertions
    // with another testing framework) and specify the former on the
    // command line for debugging.
    if (GTEST_FLAG_GET(break_on_failure)) {
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT)
      // Using DebugBreak on Windows allows gtest to still break into a debugger
      // when a failure happens and both the --gtest_break_on_failure and
      // the --gtest_catch_exceptions flags are specified.
      DebugBreak();
#elif (!defined(__native_client__)) &&            \
    ((defined(__clang__) || defined(__GNUC__)) && \
     (defined(__x86_64__) || defined(__i386__)))
      // with clang/gcc we can achieve the same effect on x86 by invoking int3
      asm("int3");
#elif GTEST_HAS_BUILTIN(__builtin_trap)
      __builtin_trap();
#elif defined(SIGTRAP)
      raise(SIGTRAP);
#else
      // Dereference nullptr through a volatile pointer to prevent the compiler
      // from removing. We use this rather than abort() or __builtin_trap() for
      // portability: some debuggers don't correctly trap abort().
      *static_cast<volatile int*>(nullptr) = 1;
#endif  // GTEST_OS_WINDOWS
    } else if (GTEST_FLAG_GET(throw_on_failure)) {
#if GTEST_HAS_EXCEPTIONS
      throw internal::GoogleTestFailureException(result);
#else
      // We cannot call abort() as it generates a pop-up in debug mode
      // that cannot be suppressed in VC 7.1 or below.
      exit(1);
#endif
    }
  }
}

// Adds a TestProperty to the current TestResult object when invoked from
// inside a test, to current TestSuite's ad_hoc_test_result_ when invoked
// from SetUpTestSuite or TearDownTestSuite, or to the global property set
// when invoked elsewhere.  If the result already contains a property with
// the same key, the value will be updated.
void UnitTest::RecordProperty(const std::string& key,
                              const std::string& value) {
  impl_->RecordProperty(TestProperty(key, value));
}

// Runs all tests in this UnitTest object and prints the result.
// Returns 0 if successful, or 1 otherwise.
//
// We don't protect this under mutex_, as we only support calling it
// from the main thread.
int UnitTest::Run() {
#ifdef GTEST_HAS_DEATH_TEST
  const bool in_death_test_child_process =
      !GTEST_FLAG_GET(internal_run_death_test).empty();

  // Google Test implements this protocol for catching that a test
  // program exits before returning control to Google Test:
  //
  //   1. Upon start, Google Test creates a file whose absolute path
  //      is specified by the environment variable
  //      TEST_PREMATURE_EXIT_FILE.
  //   2. When Google Test has finished its work, it deletes the file.
  //
  // This allows a test runner to set TEST_PREMATURE_EXIT_FILE before
  // running a Google-Test-based test program and check the existence
  // of the file at the end of the test execution to see if it has
  // exited prematurely.

  // If we are in the child process of a death test, don't
  // create/delete the premature exit file, as doing so is unnecessary
  // and will confuse the parent process.  Otherwise, create/delete
  // the file upon entering/leaving this function.  If the program
  // somehow exits before this function has a chance to return, the
  // premature-exit file will be left undeleted, causing a test runner
  // that understands the premature-exit-file protocol to report the
  // test as having failed.
  const internal::ScopedPrematureExitFile premature_exit_file(
      in_death_test_child_process
          ? nullptr
          : internal::posix::GetEnv("TEST_PREMATURE_EXIT_FILE"));
#else
  const bool in_death_test_child_process = false;
#endif  // GTEST_HAS_DEATH_TEST

  // Captures the value of GTEST_FLAG(catch_exceptions).  This value will be
  // used for the duration of the program.
  impl()->set_catch_exceptions(GTEST_FLAG_GET(catch_exceptions));

#ifdef GTEST_OS_WINDOWS
  // Either the user wants Google Test to catch exceptions thrown by the
  // tests or this is executing in the context of death test child
  // process. In either case the user does not want to see pop-up dialogs
  // about crashes - they are expected.
  if (impl()->catch_exceptions() || in_death_test_child_process) {
#if !defined(GTEST_OS_WINDOWS_MOBILE) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_GAMES)
    // SetErrorMode doesn't exist on CE.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT |
                 SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif  // !GTEST_OS_WINDOWS_MOBILE

#if (defined(_MSC_VER) || defined(GTEST_OS_WINDOWS_MINGW)) && \
    !defined(GTEST_OS_WINDOWS_MOBILE)
    // Death test children can be terminated with _abort().  On Windows,
    // _abort() can show a dialog with a warning message.  This forces the
    // abort message to go to stderr instead.
    _set_error_mode(_OUT_TO_STDERR);
#endif

#if defined(_MSC_VER) && !defined(GTEST_OS_WINDOWS_MOBILE)
    // In the debug version, Visual Studio pops up a separate dialog
    // offering a choice to debug the aborted program. We need to suppress
    // this dialog or it will pop up for every EXPECT/ASSERT_DEATH statement
    // executed. Google Test will notify the user of any unexpected
    // failure via stderr.
    if (!GTEST_FLAG_GET(break_on_failure))
      _set_abort_behavior(
          0x0,                                    // Clear the following flags:
          _WRITE_ABORT_MSG | _CALL_REPORTFAULT);  // pop-up window, core dump.

    // In debug mode, the Windows CRT can crash with an assertion over invalid
    // input (e.g. passing an invalid file descriptor).  The default handling
    // for these assertions is to pop up a dialog and wait for user input.
    // Instead ask the CRT to dump such assertions to stderr non-interactively.
    if (!IsDebuggerPresent()) {
      (void)_CrtSetReportMode(_CRT_ASSERT,
                              _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
      (void)_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    }
#endif
  }
#else
  (void)in_death_test_child_process;  // Needed inside the #if block above
#endif  // GTEST_OS_WINDOWS

  return internal::HandleExceptionsInMethodIfSupported(
             impl(), &internal::UnitTestImpl::RunAllTests,
             "auxiliary test code (environments or event listeners)")
             ? 0
             : 1;
}

#if GTEST_HAS_FILE_SYSTEM
// Returns the working directory when the first TEST() or TEST_F() was
// executed.
const char* UnitTest::original_working_dir() const {
  return impl_->original_working_dir_.c_str();
}
#endif  // GTEST_HAS_FILE_SYSTEM

// Returns the TestSuite object for the test that's currently running,
// or NULL if no test is running.
const TestSuite* UnitTest::current_test_suite() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  return impl_->current_test_suite();
}

// Legacy API is still available but deprecated
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
const TestCase* UnitTest::current_test_case() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  return impl_->current_test_suite();
}
#endif

// Returns the TestInfo object for the test that's currently running,
// or NULL if no test is running.
const TestInfo* UnitTest::current_test_info() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  return impl_->current_test_info();
}

// Returns the random seed used at the start of the current test run.
int UnitTest::random_seed() const { return impl_->random_seed(); }

// Returns ParameterizedTestSuiteRegistry object used to keep track of
// value-parameterized tests and instantiate and register them.
internal::ParameterizedTestSuiteRegistry&
UnitTest::parameterized_test_registry() GTEST_LOCK_EXCLUDED_(mutex_) {
  return impl_->parameterized_test_registry();
}

// Creates an empty UnitTest.
UnitTest::UnitTest() { impl_ = new internal::UnitTestImpl(this); }

// Destructor of UnitTest.
UnitTest::~UnitTest() { delete impl_; }

// Pushes a trace defined by SCOPED_TRACE() on to the per-thread
// Google Test trace stack.
void UnitTest::PushGTestTrace(const internal::TraceInfo& trace)
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  impl_->gtest_trace_stack().push_back(trace);
}

// Pops a trace from the per-thread Google Test trace stack.
void UnitTest::PopGTestTrace() GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  impl_->gtest_trace_stack().pop_back();
}

namespace internal {

UnitTestImpl::UnitTestImpl(UnitTest* parent)
    : parent_(parent),
      GTEST_DISABLE_MSC_WARNINGS_PUSH_(4355 /* using this in initializer */)
          default_global_test_part_result_reporter_(this),
      default_per_thread_test_part_result_reporter_(this),
      GTEST_DISABLE_MSC_WARNINGS_POP_() global_test_part_result_reporter_(
          &default_global_test_part_result_reporter_),
      per_thread_test_part_result_reporter_(
          &default_per_thread_test_part_result_reporter_),
      parameterized_test_registry_(),
      parameterized_tests_registered_(false),
      last_death_test_suite_(-1),
      current_test_suite_(nullptr),
      current_test_info_(nullptr),
      ad_hoc_test_result_(),
      os_stack_trace_getter_(nullptr),
      post_flag_parse_init_performed_(false),
      random_seed_(0),  // Will be overridden by the flag before first use.
      random_(0),       // Will be reseeded before first use.
      start_timestamp_(0),
      elapsed_time_(0),
#ifdef GTEST_HAS_DEATH_TEST
      death_test_factory_(new DefaultDeathTestFactory),
#endif
      // Will be overridden by the flag before first use.
      catch_exceptions_(false) {
  listeners()->SetDefaultResultPrinter(new PrettyUnitTestResultPrinter);
}

UnitTestImpl::~UnitTestImpl() {
  // Deletes every TestSuite.
  ForEach(test_suites_, internal::Delete<TestSuite>);

  // Deletes every Environment.
  ForEach(environments_, internal::Delete<Environment>);

  delete os_stack_trace_getter_;
}

// Adds a TestProperty to the current TestResult object when invoked in a
// context of a test, to current test suite's ad_hoc_test_result when invoke
// from SetUpTestSuite/TearDownTestSuite, or to the global property set
// otherwise.  If the result already contains a property with the same key,
// the value will be updated.
void UnitTestImpl::RecordProperty(const TestProperty& test_property) {
  std::string xml_element;
  TestResult* test_result;  // TestResult appropriate for property recording.

  if (current_test_info_ != nullptr) {
    xml_element = "testcase";
    test_result = &(current_test_info_->result_);
  } else if (current_test_suite_ != nullptr) {
    xml_element = "testsuite";
    test_result = &(current_test_suite_->ad_hoc_test_result_);
  } else {
    xml_element = "testsuites";
    test_result = &ad_hoc_test_result_;
  }
  test_result->RecordProperty(xml_element, test_property);
}

#ifdef GTEST_HAS_DEATH_TEST
// Disables event forwarding if the control is currently in a death test
// subprocess. Must not be called before InitGoogleTest.
void UnitTestImpl::SuppressTestEventsIfInSubprocess() {
  if (internal_run_death_test_flag_ != nullptr)
    listeners()->SuppressEventForwarding(true);
}
#endif  // GTEST_HAS_DEATH_TEST

// Initializes event listeners performing XML output as specified by
// UnitTestOptions. Must not be called before InitGoogleTest.
void UnitTestImpl::ConfigureXmlOutput() {
  const std::string& output_format = UnitTestOptions::GetOutputFormat();
#if GTEST_HAS_FILE_SYSTEM
  if (output_format == "xml") {
    listeners()->SetDefaultXmlGenerator(new XmlUnitTestResultPrinter(
        UnitTestOptions::GetAbsolutePathToOutputFile().c_str()));
  } else if (output_format == "json") {
    listeners()->SetDefaultXmlGenerator(new JsonUnitTestResultPrinter(
        UnitTestOptions::GetAbsolutePathToOutputFile().c_str()));
  } else if (!output_format.empty()) {
    GTEST_LOG_(WARNING) << "WARNING: unrecognized output format \""
                        << output_format << "\" ignored.";
  }
#else
  if (!output_format.empty()) {
    GTEST_LOG_(ERROR) << "ERROR: alternative output formats require "
                      << "GTEST_HAS_FILE_SYSTEM to be enabled";
  }
#endif  // GTEST_HAS_FILE_SYSTEM
}

#if GTEST_CAN_STREAM_RESULTS_
// Initializes event listeners for streaming test results in string form.
// Must not be called before InitGoogleTest.
void UnitTestImpl::ConfigureStreamingOutput() {
  const std::string& target = GTEST_FLAG_GET(stream_result_to);
  if (!target.empty()) {
    const size_t pos = target.find(':');
    if (pos != std::string::npos) {
      listeners()->Append(
          new StreamingListener(target.substr(0, pos), target.substr(pos + 1)));
    } else {
      GTEST_LOG_(WARNING) << "unrecognized streaming target \"" << target
                          << "\" ignored.";
    }
  }
}
#endif  // GTEST_CAN_STREAM_RESULTS_

// Performs initialization dependent upon flag values obtained in
// ParseGoogleTestFlagsOnly.  Is called from InitGoogleTest after the call to
// ParseGoogleTestFlagsOnly.  In case a user neglects to call InitGoogleTest
// this function is also called from RunAllTests.  Since this function can be
// called more than once, it has to be idempotent.
void UnitTestImpl::PostFlagParsingInit() {
  // Ensures that this function does not execute more than once.
  if (!post_flag_parse_init_performed_) {
    post_flag_parse_init_performed_ = true;

#if defined(GTEST_CUSTOM_TEST_EVENT_LISTENER_)
    // Register to send notifications about key process state changes.
    listeners()->Append(new GTEST_CUSTOM_TEST_EVENT_LISTENER_());
#endif  // defined(GTEST_CUSTOM_TEST_EVENT_LISTENER_)

#ifdef GTEST_HAS_DEATH_TEST
    InitDeathTestSubprocessControlInfo();
    SuppressTestEventsIfInSubprocess();
#endif  // GTEST_HAS_DEATH_TEST

    // Registers parameterized tests. This makes parameterized tests
    // available to the UnitTest reflection API without running
    // RUN_ALL_TESTS.
    RegisterParameterizedTests();

    // Configures listeners for XML output. This makes it possible for users
    // to shut down the default XML output before invoking RUN_ALL_TESTS.
    ConfigureXmlOutput();

    if (GTEST_FLAG_GET(brief)) {
      listeners()->SetDefaultResultPrinter(new BriefUnitTestResultPrinter);
    }

#if GTEST_CAN_STREAM_RESULTS_
    // Configures listeners for streaming test results to the specified server.
    ConfigureStreamingOutput();
#endif  // GTEST_CAN_STREAM_RESULTS_

#ifdef GTEST_HAS_ABSL
    if (GTEST_FLAG_GET(install_failure_signal_handler)) {
      absl::FailureSignalHandlerOptions options;
      absl::InstallFailureSignalHandler(options);
    }
#endif  // GTEST_HAS_ABSL
  }
}

// Finds and returns a TestSuite with the given name.  If one doesn't
// exist, creates one and returns it.  It's the CALLER'S
// RESPONSIBILITY to ensure that this function is only called WHEN THE
// TESTS ARE NOT SHUFFLED.
//
// Arguments:
//
//   test_suite_name: name of the test suite
//   type_param:      the name of the test suite's type parameter, or NULL if
//                    this is not a typed or a type-parameterized test suite.
//   set_up_tc:       pointer to the function that sets up the test suite
//   tear_down_tc:    pointer to the function that tears down the test suite
TestSuite* UnitTestImpl::GetTestSuite(
    const std::string& test_suite_name, const char* type_param,
    internal::SetUpTestSuiteFunc set_up_tc,
    internal::TearDownTestSuiteFunc tear_down_tc) {
  // During initialization, all TestInfos for a given suite are added in
  // sequence. To optimize this case, see if the most recently added suite is
  // the one being requested now.
  if (!test_suites_.empty() &&
      (*test_suites_.rbegin())->name_ == test_suite_name) {
    return *test_suites_.rbegin();
  }

  // Fall back to searching the collection.
  auto item_it = test_suites_by_name_.find(test_suite_name);
  if (item_it != test_suites_by_name_.end()) {
    return item_it->second;
  }

  // Not found. Create a new instance.
  auto* const new_test_suite =
      new TestSuite(test_suite_name, type_param, set_up_tc, tear_down_tc);
  test_suites_by_name_.emplace(test_suite_name, new_test_suite);

  const UnitTestFilter death_test_suite_filter(kDeathTestSuiteFilter);
  // Is this a death test suite?
  if (death_test_suite_filter.MatchesName(test_suite_name)) {
    // Yes.  Inserts the test suite after the last death test suite
    // defined so far.  This only works when the test suites haven't
    // been shuffled.  Otherwise we may end up running a death test
    // after a non-death test.
    ++last_death_test_suite_;
    test_suites_.insert(test_suites_.begin() + last_death_test_suite_,
                        new_test_suite);
  } else {
    // No.  Appends to the end of the list.
    test_suites_.push_back(new_test_suite);
  }

  test_suite_indices_.push_back(static_cast<int>(test_suite_indices_.size()));
  return new_test_suite;
}

// Helpers for setting up / tearing down the given environment.  They
// are for use in the ForEach() function.
static void SetUpEnvironment(Environment* env) { env->SetUp(); }
static void TearDownEnvironment(Environment* env) { env->TearDown(); }

// Runs all tests in this UnitTest object, prints the result, and
// returns true if all tests are successful.  If any exception is
// thrown during a test, the test is considered to be failed, but the
// rest of the tests will still be run.
//
// When parameterized tests are enabled, it expands and registers
// parameterized tests first in RegisterParameterizedTests().
// All other functions called from RunAllTests() may safely assume that
// parameterized tests are ready to be counted and run.
bool UnitTestImpl::RunAllTests() {
  // True if and only if Google Test is initialized before RUN_ALL_TESTS() is
  // called.
  const bool gtest_is_initialized_before_run_all_tests = GTestIsInitialized();

  // Do not run any test if the --help flag was specified.
  if (g_help_flag) return true;

  // Repeats the call to the post-flag parsing initialization in case the
  // user didn't call InitGoogleTest.
  PostFlagParsingInit();

#if GTEST_HAS_FILE_SYSTEM
  // Even if sharding is not on, test runners may want to use the
  // GTEST_SHARD_STATUS_FILE to query whether the test supports the sharding
  // protocol.
  internal::WriteToShardStatusFileIfNeeded();
#endif  // GTEST_HAS_FILE_SYSTEM

  // True if and only if we are in a subprocess for running a thread-safe-style
  // death test.
  bool in_subprocess_for_death_test = false;

#ifdef GTEST_HAS_DEATH_TEST
  in_subprocess_for_death_test = (internal_run_death_test_flag_ != nullptr);
#if defined(GTEST_EXTRA_DEATH_TEST_CHILD_SETUP_)
  if (in_subprocess_for_death_test) {
    GTEST_EXTRA_DEATH_TEST_CHILD_SETUP_();
  }
#endif  // defined(GTEST_EXTRA_DEATH_TEST_CHILD_SETUP_)
#endif  // GTEST_HAS_DEATH_TEST

  const bool should_shard = ShouldShard(kTestTotalShards, kTestShardIndex,
                                        in_subprocess_for_death_test);

  // Compares the full test names with the filter to decide which
  // tests to run.
  const bool has_tests_to_run =
      FilterTests(should_shard ? HONOR_SHARDING_PROTOCOL
                               : IGNORE_SHARDING_PROTOCOL) > 0;

  // Lists the tests and exits if the --gtest_list_tests flag was specified.
  if (GTEST_FLAG_GET(list_tests)) {
    // This must be called *after* FilterTests() has been called.
    ListTestsMatchingFilter();
    return true;
  }

  random_seed_ = GetRandomSeedFromFlag(GTEST_FLAG_GET(random_seed));

  // True if and only if at least one test has failed.
  bool failed = false;

  TestEventListener* repeater = listeners()->repeater();

  start_timestamp_ = GetTimeInMillis();
  repeater->OnTestProgramStart(*parent_);

  // How many times to repeat the tests?  We don't want to repeat them
  // when we are inside the subprocess of a death test.
  const int repeat = in_subprocess_for_death_test ? 1 : GTEST_FLAG_GET(repeat);

  // Repeats forever if the repeat count is negative.
  const bool gtest_repeat_forever = repeat < 0;

  // Should test environments be set up and torn down for each repeat, or only
  // set up on the first and torn down on the last iteration? If there is no
  // "last" iteration because the tests will repeat forever, always recreate the
  // environments to avoid leaks in case one of the environments is using
  // resources that are external to this process. Without this check there would
  // be no way to clean up those external resources automatically.
  const bool recreate_environments_when_repeating =
      GTEST_FLAG_GET(recreate_environments_when_repeating) ||
      gtest_repeat_forever;

  for (int i = 0; gtest_repeat_forever || i != repeat; i++) {
    // We want to preserve failures generated by ad-hoc test
    // assertions executed before RUN_ALL_TESTS().
    ClearNonAdHocTestResult();

    Timer timer;

    // Shuffles test suites and tests if requested.
    if (has_tests_to_run && GTEST_FLAG_GET(shuffle)) {
      random()->Reseed(static_cast<uint32_t>(random_seed_));
      // This should be done before calling OnTestIterationStart(),
      // such that a test event listener can see the actual test order
      // in the event.
      ShuffleTests();
    }

    // Tells the unit test event listeners that the tests are about to start.
    repeater->OnTestIterationStart(*parent_, i);

    // Runs each test suite if there is at least one test to run.
    if (has_tests_to_run) {
      // Sets up all environments beforehand. If test environments aren't
      // recreated for each iteration, only do so on the first iteration.
      if (i == 0 || recreate_environments_when_repeating) {
        repeater->OnEnvironmentsSetUpStart(*parent_);
        ForEach(environments_, SetUpEnvironment);
        repeater->OnEnvironmentsSetUpEnd(*parent_);
      }

      // Runs the tests only if there was no fatal failure or skip triggered
      // during global set-up.
      if (Test::IsSkipped()) {
        // Emit diagnostics when global set-up calls skip, as it will not be
        // emitted by default.
        TestResult& test_result =
            *internal::GetUnitTestImpl()->current_test_result();
        for (int j = 0; j < test_result.total_part_count(); ++j) {
          const TestPartResult& test_part_result =
              test_result.GetTestPartResult(j);
          if (test_part_result.type() == TestPartResult::kSkip) {
            const std::string& result = test_part_result.message();
            printf("%s\n", result.c_str());
          }
        }
        fflush(stdout);
      } else if (!Test::HasFatalFailure()) {
        for (int test_index = 0; test_index < total_test_suite_count();
             test_index++) {
          GetMutableSuiteCase(test_index)->Run();
          if (GTEST_FLAG_GET(fail_fast) &&
              GetMutableSuiteCase(test_index)->Failed()) {
            for (int j = test_index + 1; j < total_test_suite_count(); j++) {
              GetMutableSuiteCase(j)->Skip();
            }
            break;
          }
        }
      } else if (Test::HasFatalFailure()) {
        // If there was a fatal failure during the global setup then we know we
        // aren't going to run any tests. Explicitly mark all of the tests as
        // skipped to make this obvious in the output.
        for (int test_index = 0; test_index < total_test_suite_count();
             test_index++) {
          GetMutableSuiteCase(test_index)->Skip();
        }
      }

      // Tears down all environments in reverse order afterwards. If test
      // environments aren't recreated for each iteration, only do so on the
      // last iteration.
      if (i == repeat - 1 || recreate_environments_when_repeating) {
        repeater->OnEnvironmentsTearDownStart(*parent_);
        std::for_each(environments_.rbegin(), environments_.rend(),
                      TearDownEnvironment);
        repeater->OnEnvironmentsTearDownEnd(*parent_);
      }
    }

    elapsed_time_ = timer.Elapsed();

    // Tells the unit test event listener that the tests have just finished.
    repeater->OnTestIterationEnd(*parent_, i);

    // Gets the result and clears it.
    if (!Passed()) {
      failed = true;
    }

    // Restores the original test order after the iteration.  This
    // allows the user to quickly repro a failure that happens in the
    // N-th iteration without repeating the first (N - 1) iterations.
    // This is not enclosed in "if (GTEST_FLAG(shuffle)) { ... }", in
    // case the user somehow changes the value of the flag somewhere
    // (it's always safe to unshuffle the tests).
    UnshuffleTests();

    if (GTEST_FLAG_GET(shuffle)) {
      // Picks a new random seed for each iteration.
      random_seed_ = GetNextRandomSeed(random_seed_);
    }
  }

  repeater->OnTestProgramEnd(*parent_);
  // Destroy environments in normal code, not in static teardown.
  bool delete_environment_on_teardown = true;
  if (delete_environment_on_teardown) {
    ForEach(environments_, internal::Delete<Environment>);
    environments_.clear();
  }

  if (!gtest_is_initialized_before_run_all_tests) {
    ColoredPrintf(
        GTestColor::kRed,
        "\nIMPORTANT NOTICE - DO NOT IGNORE:\n"
        "This test program did NOT call " GTEST_INIT_GOOGLE_TEST_NAME_
        "() before calling RUN_ALL_TESTS(). This is INVALID. Soon " GTEST_NAME_
        " will start to enforce the valid usage. "
        "Please fix it ASAP, or IT WILL START TO FAIL.\n");  // NOLINT
  }

  return !failed;
}

#if GTEST_HAS_FILE_SYSTEM
// Reads the GTEST_SHARD_STATUS_FILE environment variable, and creates the file
// if the variable is present. If a file already exists at this location, this
// function will write over it. If the variable is present, but the file cannot
// be created, prints an error and exits.
void WriteToShardStatusFileIfNeeded() {
  const char* const test_shard_file = posix::GetEnv(kTestShardStatusFile);
  if (test_shard_file != nullptr) {
    FILE* const file = posix::FOpen(test_shard_file, "w");
    if (file == nullptr) {
      ColoredPrintf(GTestColor::kRed,
                    "Could not write to the test shard status file \"%s\" "
                    "specified by the %s environment variable.\n",
                    test_shard_file, kTestShardStatusFile);
      fflush(stdout);
      exit(EXIT_FAILURE);
    }
    fclose(file);
  }
}
#endif  // GTEST_HAS_FILE_SYSTEM

// Checks whether sharding is enabled by examining the relevant
// environment variable values. If the variables are present,
// but inconsistent (i.e., shard_index >= total_shards), prints
// an error and exits. If in_subprocess_for_death_test, sharding is
// disabled because it must only be applied to the original test
// process. Otherwise, we could filter out death tests we intended to execute.
bool ShouldShard(const char* total_shards_env, const char* shard_index_env,
                 bool in_subprocess_for_death_test) {
  if (in_subprocess_for_death_test) {
    return false;
  }

  const int32_t total_shards = Int32FromEnvOrDie(total_shards_env, -1);
  const int32_t shard_index = Int32FromEnvOrDie(shard_index_env, -1);

  if (total_shards == -1 && shard_index == -1) {
    return false;
  } else if (total_shards == -1 && shard_index != -1) {
    const Message msg = Message() << "Invalid environment variables: you have "
                                  << kTestShardIndex << " = " << shard_index
                                  << ", but have left " << kTestTotalShards
                                  << " unset.\n";
    ColoredPrintf(GTestColor::kRed, "%s", msg.GetString().c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
  } else if (total_shards != -1 && shard_index == -1) {
    const Message msg = Message()
                        << "Invalid environment variables: you have "
                        << kTestTotalShards << " = " << total_shards
                        << ", but have left " << kTestShardIndex << " unset.\n";
    ColoredPrintf(GTestColor::kRed, "%s", msg.GetString().c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
  } else if (shard_index < 0 || shard_index >= total_shards) {
    const Message msg =
        Message() << "Invalid environment variables: we require 0 <= "
                  << kTestShardIndex << " < " << kTestTotalShards
                  << ", but you have " << kTestShardIndex << "=" << shard_index
                  << ", " << kTestTotalShards << "=" << total_shards << ".\n";
    ColoredPrintf(GTestColor::kRed, "%s", msg.GetString().c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
  }

  return total_shards > 1;
}

// Parses the environment variable var as an Int32. If it is unset,
// returns default_val. If it is not an Int32, prints an error
// and aborts.
int32_t Int32FromEnvOrDie(const char* var, int32_t default_val) {
  const char* str_val = posix::GetEnv(var);
  if (str_val == nullptr) {
    return default_val;
  }

  int32_t result;
  if (!ParseInt32(Message() << "The value of environment variable " << var,
                  str_val, &result)) {
    exit(EXIT_FAILURE);
  }
  return result;
}

// Given the total number of shards, the shard index, and the test id,
// returns true if and only if the test should be run on this shard. The test id
// is some arbitrary but unique non-negative integer assigned to each test
// method. Assumes that 0 <= shard_index < total_shards.
bool ShouldRunTestOnShard(int total_shards, int shard_index, int test_id) {
  return (test_id % total_shards) == shard_index;
}

// Compares the name of each test with the user-specified filter to
// decide whether the test should be run, then records the result in
// each TestSuite and TestInfo object.
// If shard_tests == true, further filters tests based on sharding
// variables in the environment - see
// https://github.com/google/googletest/blob/main/docs/advanced.md
// . Returns the number of tests that should run.
int UnitTestImpl::FilterTests(ReactionToSharding shard_tests) {
  const int32_t total_shards = shard_tests == HONOR_SHARDING_PROTOCOL
                                   ? Int32FromEnvOrDie(kTestTotalShards, -1)
                                   : -1;
  const int32_t shard_index = shard_tests == HONOR_SHARDING_PROTOCOL
                                  ? Int32FromEnvOrDie(kTestShardIndex, -1)
                                  : -1;

  const PositiveAndNegativeUnitTestFilter gtest_flag_filter(
      GTEST_FLAG_GET(filter));
  const UnitTestFilter disable_test_filter(kDisableTestFilter);
  // num_runnable_tests are the number of tests that will
  // run across all shards (i.e., match filter and are not disabled).
  // num_selected_tests are the number of tests to be run on
  // this shard.
  int num_runnable_tests = 0;
  int num_selected_tests = 0;
  for (auto* test_suite : test_suites_) {
    const std::string& test_suite_name = test_suite->name_;
    test_suite->set_should_run(false);

    for (TestInfo* test_info : test_suite->test_info_list()) {
      const std::string& test_name = test_info->name_;
      // A test is disabled if test suite name or test name matches
      // kDisableTestFilter.
      const bool is_disabled =
          disable_test_filter.MatchesName(test_suite_name) ||
          disable_test_filter.MatchesName(test_name);
      test_info->is_disabled_ = is_disabled;

      const bool matches_filter =
          gtest_flag_filter.MatchesTest(test_suite_name, test_name);
      test_info->matches_filter_ = matches_filter;

      const bool is_runnable =
          (GTEST_FLAG_GET(also_run_disabled_tests) || !is_disabled) &&
          matches_filter;

      const bool is_in_another_shard =
          shard_tests != IGNORE_SHARDING_PROTOCOL &&
          !ShouldRunTestOnShard(total_shards, shard_index, num_runnable_tests);
      test_info->is_in_another_shard_ = is_in_another_shard;
      const bool is_selected = is_runnable && !is_in_another_shard;

      num_runnable_tests += is_runnable;
      num_selected_tests += is_selected;

      test_info->should_run_ = is_selected;
      test_suite->set_should_run(test_suite->should_run() || is_selected);
    }
  }
  return num_selected_tests;
}

// Prints the given C-string on a single line by replacing all '\n'
// characters with string "\\n".  If the output takes more than
// max_length characters, only prints the first max_length characters
// and "...".
static void PrintOnOneLine(const char* str, int max_length) {
  if (str != nullptr) {
    for (int i = 0; *str != '\0'; ++str) {
      if (i >= max_length) {
        printf("...");
        break;
      }
      if (*str == '\n') {
        printf("\\n");
        i += 2;
      } else {
        printf("%c", *str);
        ++i;
      }
    }
  }
}

// Prints the names of the tests matching the user-specified filter flag.
void UnitTestImpl::ListTestsMatchingFilter() {
  // Print at most this many characters for each type/value parameter.
  const int kMaxParamLength = 250;

  for (auto* test_suite : test_suites_) {
    bool printed_test_suite_name = false;

    for (size_t j = 0; j < test_suite->test_info_list().size(); j++) {
      const TestInfo* const test_info = test_suite->test_info_list()[j];
      if (test_info->matches_filter_) {
        if (!printed_test_suite_name) {
          printed_test_suite_name = true;
          printf("%s.", test_suite->name());
          if (test_suite->type_param() != nullptr) {
            printf("  # %s = ", kTypeParamLabel);
            // We print the type parameter on a single line to make
            // the output easy to parse by a program.
            PrintOnOneLine(test_suite->type_param(), kMaxParamLength);
          }
          printf("\n");
        }
        printf("  %s", test_info->name());
        if (test_info->value_param() != nullptr) {
          printf("  # %s = ", kValueParamLabel);
          // We print the value parameter on a single line to make the
          // output easy to parse by a program.
          PrintOnOneLine(test_info->value_param(), kMaxParamLength);
        }
        printf("\n");
      }
    }
  }
  fflush(stdout);
#if GTEST_HAS_FILE_SYSTEM
  const std::string& output_format = UnitTestOptions::GetOutputFormat();
  if (output_format == "xml" || output_format == "json") {
    FILE* fileout =
        OpenFileForWriting(UnitTestOptions::GetAbsolutePathToOutputFile());
    std::stringstream stream;
    if (output_format == "xml") {
      XmlUnitTestResultPrinter(
          UnitTestOptions::GetAbsolutePathToOutputFile().c_str())
          .PrintXmlTestsList(&stream, test_suites_);
    } else if (output_format == "json") {
      JsonUnitTestResultPrinter(
          UnitTestOptions::GetAbsolutePathToOutputFile().c_str())
          .PrintJsonTestList(&stream, test_suites_);
    }
    fprintf(fileout, "%s", StringStreamToString(&stream).c_str());
    fclose(fileout);
  }
#endif  // GTEST_HAS_FILE_SYSTEM
}

// Sets the OS stack trace getter.
//
// Does nothing if the input and the current OS stack trace getter are
// the same; otherwise, deletes the old getter and makes the input the
// current getter.
void UnitTestImpl::set_os_stack_trace_getter(
    OsStackTraceGetterInterface* getter) {
  if (os_stack_trace_getter_ != getter) {
    delete os_stack_trace_getter_;
    os_stack_trace_getter_ = getter;
  }
}

// Returns the current OS stack trace getter if it is not NULL;
// otherwise, creates an OsStackTraceGetter, makes it the current
// getter, and returns it.
OsStackTraceGetterInterface* UnitTestImpl::os_stack_trace_getter() {
  if (os_stack_trace_getter_ == nullptr) {
#ifdef GTEST_OS_STACK_TRACE_GETTER_
    os_stack_trace_getter_ = new GTEST_OS_STACK_TRACE_GETTER_;
#else
    os_stack_trace_getter_ = new OsStackTraceGetter;
#endif  // GTEST_OS_STACK_TRACE_GETTER_
  }

  return os_stack_trace_getter_;
}

// Returns the most specific TestResult currently running.
TestResult* UnitTestImpl::current_test_result() {
  if (current_test_info_ != nullptr) {
    return &current_test_info_->result_;
  }
  if (current_test_suite_ != nullptr) {
    return &current_test_suite_->ad_hoc_test_result_;
  }
  return &ad_hoc_test_result_;
}

// Shuffles all test suites, and the tests within each test suite,
// making sure that death tests are still run first.
void UnitTestImpl::ShuffleTests() {
  // Shuffles the death test suites.
  ShuffleRange(random(), 0, last_death_test_suite_ + 1, &test_suite_indices_);

  // Shuffles the non-death test suites.
  ShuffleRange(random(), last_death_test_suite_ + 1,
               static_cast<int>(test_suites_.size()), &test_suite_indices_);

  // Shuffles the tests inside each test suite.
  for (auto& test_suite : test_suites_) {
    test_suite->ShuffleTests(random());
  }
}

// Restores the test suites and tests to their order before the first shuffle.
void UnitTestImpl::UnshuffleTests() {
  for (size_t i = 0; i < test_suites_.size(); i++) {
    // Unshuffles the tests in each test suite.
    test_suites_[i]->UnshuffleTests();
    // Resets the index of each test suite.
    test_suite_indices_[i] = static_cast<int>(i);
  }
}

// Returns the current OS stack trace as an std::string.
//
// The maximum number of stack frames to be included is specified by
// the gtest_stack_trace_depth flag.  The skip_count parameter
// specifies the number of top frames to be skipped, which doesn't
// count against the number of frames to be included.
//
// For example, if Foo() calls Bar(), which in turn calls
// GetCurrentOsStackTraceExceptTop(..., 1), Foo() will be included in
// the trace but Bar() and GetCurrentOsStackTraceExceptTop() won't.
GTEST_NO_INLINE_ GTEST_NO_TAIL_CALL_ std::string
GetCurrentOsStackTraceExceptTop(int skip_count) {
  // We pass skip_count + 1 to skip this wrapper function in addition
  // to what the user really wants to skip.
  return GetUnitTestImpl()->CurrentOsStackTraceExceptTop(skip_count + 1);
}

// Used by the GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_ macro to
// suppress unreachable code warnings.
namespace {
class ClassUniqueToAlwaysTrue {};
}  // namespace

bool IsTrue(bool condition) { return condition; }

bool AlwaysTrue() {
#if GTEST_HAS_EXCEPTIONS
  // This condition is always false so AlwaysTrue() never actually throws,
  // but it makes the compiler think that it may throw.
  if (IsTrue(false)) throw ClassUniqueToAlwaysTrue();
#endif  // GTEST_HAS_EXCEPTIONS
  return true;
}

// If *pstr starts with the given prefix, modifies *pstr to be right
// past the prefix and returns true; otherwise leaves *pstr unchanged
// and returns false.  None of pstr, *pstr, and prefix can be NULL.
bool SkipPrefix(const char* prefix, const char** pstr) {
  const size_t prefix_len = strlen(prefix);
  if (strncmp(*pstr, prefix, prefix_len) == 0) {
    *pstr += prefix_len;
    return true;
  }
  return false;
}

// Parses a string as a command line flag.  The string should have
// the format "--flag=value".  When def_optional is true, the "=value"
// part can be omitted.
//
// Returns the value of the flag, or NULL if the parsing failed.
static const char* ParseFlagValue(const char* str, const char* flag_name,
                                  bool def_optional) {
  // str and flag must not be NULL.
  if (str == nullptr || flag_name == nullptr) return nullptr;

  // The flag must start with "--" followed by GTEST_FLAG_PREFIX_.
  const std::string flag_str =
      std::string("--") + GTEST_FLAG_PREFIX_ + flag_name;
  const size_t flag_len = flag_str.length();
  if (strncmp(str, flag_str.c_str(), flag_len) != 0) return nullptr;

  // Skips the flag name.
  const char* flag_end = str + flag_len;

  // When def_optional is true, it's OK to not have a "=value" part.
  if (def_optional && (flag_end[0] == '\0')) {
    return flag_end;
  }

  // If def_optional is true and there are more characters after the
  // flag name, or if def_optional is false, there must be a '=' after
  // the flag name.
  if (flag_end[0] != '=') return nullptr;

  // Returns the string after "=".
  return flag_end + 1;
}

// Parses a string for a bool flag, in the form of either
// "--flag=value" or "--flag".
//
// In the former case, the value is taken as true as long as it does
// not start with '0', 'f', or 'F'.
//
// In the latter case, the value is taken as true.
//
// On success, stores the value of the flag in *value, and returns
// true.  On failure, returns false without changing *value.
static bool ParseFlag(const char* str, const char* flag_name, bool* value) {
  // Gets the value of the flag as a string.
  const char* const value_str = ParseFlagValue(str, flag_name, true);

  // Aborts if the parsing failed.
  if (value_str == nullptr) return false;

  // Converts the string value to a bool.
  *value = !(*value_str == '0' || *value_str == 'f' || *value_str == 'F');
  return true;
}

// Parses a string for an int32_t flag, in the form of "--flag=value".
//
// On success, stores the value of the flag in *value, and returns
// true.  On failure, returns false without changing *value.
bool ParseFlag(const char* str, const char* flag_name, int32_t* value) {
  // Gets the value of the flag as a string.
  const char* const value_str = ParseFlagValue(str, flag_name, false);

  // Aborts if the parsing failed.
  if (value_str == nullptr) return false;

  // Sets *value to the value of the flag.
  return ParseInt32(Message() << "The value of flag --" << flag_name, value_str,
                    value);
}

// Parses a string for a string flag, in the form of "--flag=value".
//
// On success, stores the value of the flag in *value, and returns
// true.  On failure, returns false without changing *value.
template <typename String>
static bool ParseFlag(const char* str, const char* flag_name, String* value) {
  // Gets the value of the flag as a string.
  const char* const value_str = ParseFlagValue(str, flag_name, false);

  // Aborts if the parsing failed.
  if (value_str == nullptr) return false;

  // Sets *value to the value of the flag.
  *value = value_str;
  return true;
}

// Determines whether a string has a prefix that Google Test uses for its
// flags, i.e., starts with GTEST_FLAG_PREFIX_ or GTEST_FLAG_PREFIX_DASH_.
// If Google Test detects that a command line flag has its prefix but is not
// recognized, it will print its help message. Flags starting with
// GTEST_INTERNAL_PREFIX_ followed by "internal_" are considered Google Test
// internal flags and do not trigger the help message.
static bool HasGoogleTestFlagPrefix(const char* str) {
  return (SkipPrefix("--", &str) || SkipPrefix("-", &str) ||
          SkipPrefix("/", &str)) &&
         !SkipPrefix(GTEST_FLAG_PREFIX_ "internal_", &str) &&
         (SkipPrefix(GTEST_FLAG_PREFIX_, &str) ||
          SkipPrefix(GTEST_FLAG_PREFIX_DASH_, &str));
}

// Prints a string containing code-encoded text.  The following escape
// sequences can be used in the string to control the text color:
//
//   @@    prints a single '@' character.
//   @R    changes the color to red.
//   @G    changes the color to green.
//   @Y    changes the color to yellow.
//   @D    changes to the default terminal text color.
//
static void PrintColorEncoded(const char* str) {
  GTestColor color = GTestColor::kDefault;  // The current color.

  // Conceptually, we split the string into segments divided by escape
  // sequences.  Then we print one segment at a time.  At the end of
  // each iteration, the str pointer advances to the beginning of the
  // next segment.
  for (;;) {
    const char* p = strchr(str, '@');
    if (p == nullptr) {
      ColoredPrintf(color, "%s", str);
      return;
    }

    ColoredPrintf(color, "%s", std::string(str, p).c_str());

    const char ch = p[1];
    str = p + 2;
    if (ch == '@') {
      ColoredPrintf(color, "@");
    } else if (ch == 'D') {
      color = GTestColor::kDefault;
    } else if (ch == 'R') {
      color = GTestColor::kRed;
    } else if (ch == 'G') {
      color = GTestColor::kGreen;
    } else if (ch == 'Y') {
      color = GTestColor::kYellow;
    } else {
      --str;
    }
  }
}

static const char kColorEncodedHelpMessage[] =
    "This program contains tests written using " GTEST_NAME_
    ". You can use the\n"
    "following command line flags to control its behavior:\n"
    "\n"
    "Test Selection:\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "list_tests@D\n"
    "      List the names of all tests instead of running them. The name of\n"
    "      TEST(Foo, Bar) is \"Foo.Bar\".\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "filter=@YPOSITIVE_PATTERNS"
    "[@G-@YNEGATIVE_PATTERNS]@D\n"
    "      Run only the tests whose name matches one of the positive patterns "
    "but\n"
    "      none of the negative patterns. '?' matches any single character; "
    "'*'\n"
    "      matches any substring; ':' separates two patterns.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "also_run_disabled_tests@D\n"
    "      Run all disabled tests too.\n"
    "\n"
    "Test Execution:\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "repeat=@Y[COUNT]@D\n"
    "      Run the tests repeatedly; use a negative count to repeat forever.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "shuffle@D\n"
    "      Randomize tests' orders on every iteration.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "random_seed=@Y[NUMBER]@D\n"
    "      Random number seed to use for shuffling test orders (between 1 and\n"
    "      99999, or 0 to use a seed based on the current time).\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "recreate_environments_when_repeating@D\n"
    "      Sets up and tears down the global test environment on each repeat\n"
    "      of the test.\n"
    "\n"
    "Test Output:\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "color=@Y(@Gyes@Y|@Gno@Y|@Gauto@Y)@D\n"
    "      Enable/disable colored output. The default is @Gauto@D.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "brief=1@D\n"
    "      Only print test failures.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "print_time=0@D\n"
    "      Don't print the elapsed time of each test.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "output=@Y(@Gjson@Y|@Gxml@Y)[@G:@YDIRECTORY_PATH@G" GTEST_PATH_SEP_
    "@Y|@G:@YFILE_PATH]@D\n"
    "      Generate a JSON or XML report in the given directory or with the "
    "given\n"
    "      file name. @YFILE_PATH@D defaults to @Gtest_detail.xml@D.\n"
#if GTEST_CAN_STREAM_RESULTS_
    "  @G--" GTEST_FLAG_PREFIX_
    "stream_result_to=@YHOST@G:@YPORT@D\n"
    "      Stream test results to the given server.\n"
#endif  // GTEST_CAN_STREAM_RESULTS_
    "\n"
    "Assertion Behavior:\n"
#if defined(GTEST_HAS_DEATH_TEST) && !defined(GTEST_OS_WINDOWS)
    "  @G--" GTEST_FLAG_PREFIX_
    "death_test_style=@Y(@Gfast@Y|@Gthreadsafe@Y)@D\n"
    "      Set the default death test style.\n"
#endif  // GTEST_HAS_DEATH_TEST && !GTEST_OS_WINDOWS
    "  @G--" GTEST_FLAG_PREFIX_
    "break_on_failure@D\n"
    "      Turn assertion failures into debugger break-points.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "throw_on_failure@D\n"
    "      Turn assertion failures into C++ exceptions for use by an external\n"
    "      test framework.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "catch_exceptions=0@D\n"
    "      Do not report exceptions as test failures. Instead, allow them\n"
    "      to crash the program or throw a pop-up (on Windows).\n"
    "\n"
    "Except for @G--" GTEST_FLAG_PREFIX_
    "list_tests@D, you can alternatively set "
    "the corresponding\n"
    "environment variable of a flag (all letters in upper-case). For example, "
    "to\n"
    "disable colored text output, you can either specify "
    "@G--" GTEST_FLAG_PREFIX_
    "color=no@D or set\n"
    "the @G" GTEST_FLAG_PREFIX_UPPER_
    "COLOR@D environment variable to @Gno@D.\n"
    "\n"
    "For more information, please read the " GTEST_NAME_
    " documentation at\n"
    "@G" GTEST_PROJECT_URL_ "@D. If you find a bug in " GTEST_NAME_
    "\n"
    "(not one in your own code or tests), please report it to\n"
    "@G<" GTEST_DEV_EMAIL_ ">@D.\n";

static bool ParseGoogleTestFlag(const char* const arg) {
#define GTEST_INTERNAL_PARSE_FLAG(flag_name)  \
  do {                                        \
    auto value = GTEST_FLAG_GET(flag_name);   \
    if (ParseFlag(arg, #flag_name, &value)) { \
      GTEST_FLAG_SET(flag_name, value);       \
      return true;                            \
    }                                         \
  } while (false)

  GTEST_INTERNAL_PARSE_FLAG(also_run_disabled_tests);
  GTEST_INTERNAL_PARSE_FLAG(break_on_failure);
  GTEST_INTERNAL_PARSE_FLAG(catch_exceptions);
  GTEST_INTERNAL_PARSE_FLAG(color);
  GTEST_INTERNAL_PARSE_FLAG(death_test_style);
  GTEST_INTERNAL_PARSE_FLAG(death_test_use_fork);
  GTEST_INTERNAL_PARSE_FLAG(fail_fast);
  GTEST_INTERNAL_PARSE_FLAG(filter);
  GTEST_INTERNAL_PARSE_FLAG(internal_run_death_test);
  GTEST_INTERNAL_PARSE_FLAG(list_tests);
  GTEST_INTERNAL_PARSE_FLAG(output);
  GTEST_INTERNAL_PARSE_FLAG(brief);
  GTEST_INTERNAL_PARSE_FLAG(print_time);
  GTEST_INTERNAL_PARSE_FLAG(print_utf8);
  GTEST_INTERNAL_PARSE_FLAG(random_seed);
  GTEST_INTERNAL_PARSE_FLAG(repeat);
  GTEST_INTERNAL_PARSE_FLAG(recreate_environments_when_repeating);
  GTEST_INTERNAL_PARSE_FLAG(shuffle);
  GTEST_INTERNAL_PARSE_FLAG(stack_trace_depth);
  GTEST_INTERNAL_PARSE_FLAG(stream_result_to);
  GTEST_INTERNAL_PARSE_FLAG(throw_on_failure);
  return false;
}

#if GTEST_USE_OWN_FLAGFILE_FLAG_ && GTEST_HAS_FILE_SYSTEM
static void LoadFlagsFromFile(const std::string& path) {
  FILE* flagfile = posix::FOpen(path.c_str(), "r");
  if (!flagfile) {
    GTEST_LOG_(FATAL) << "Unable to open file \"" << GTEST_FLAG_GET(flagfile)
                      << "\"";
  }
  std::string contents(ReadEntireFile(flagfile));
  posix::FClose(flagfile);
  std::vector<std::string> lines;
  SplitString(contents, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].empty()) continue;
    if (!ParseGoogleTestFlag(lines[i].c_str())) g_help_flag = true;
  }
}
#endif  // GTEST_USE_OWN_FLAGFILE_FLAG_ && GTEST_HAS_FILE_SYSTEM

// Parses the command line for Google Test flags, without initializing
// other parts of Google Test.  The type parameter CharType can be
// instantiated to either char or wchar_t.
template <typename CharType>
void ParseGoogleTestFlagsOnlyImpl(int* argc, CharType** argv) {
  std::string flagfile_value;
  for (int i = 1; i < *argc; i++) {
    const std::string arg_string = StreamableToString(argv[i]);
    const char* const arg = arg_string.c_str();

    using internal::ParseFlag;

    bool remove_flag = false;
    if (ParseGoogleTestFlag(arg)) {
      remove_flag = true;
#if GTEST_USE_OWN_FLAGFILE_FLAG_ && GTEST_HAS_FILE_SYSTEM
    } else if (ParseFlag(arg, "flagfile", &flagfile_value)) {
      GTEST_FLAG_SET(flagfile, flagfile_value);
      LoadFlagsFromFile(flagfile_value);
      remove_flag = true;
#endif  // GTEST_USE_OWN_FLAGFILE_FLAG_ && GTEST_HAS_FILE_SYSTEM
    } else if (arg_string == "--help" || HasGoogleTestFlagPrefix(arg)) {
      // Both help flag and unrecognized Google Test flags (excluding
      // internal ones) trigger help display.
      g_help_flag = true;
    }

    if (remove_flag) {
      // Shift the remainder of the argv list left by one.
      for (int j = i + 1; j < *argc; ++j) {
        argv[j - 1] = argv[j];
      }

      // Decrements the argument count.
      (*argc)--;

      // Terminate the array with nullptr.
      argv[*argc] = nullptr;

      // We also need to decrement the iterator as we just removed
      // an element.
      i--;
    }
  }

  if (g_help_flag) {
    // We print the help here instead of in RUN_ALL_TESTS(), as the
    // latter may not be called at all if the user is using Google
    // Test with another testing framework.
    PrintColorEncoded(kColorEncodedHelpMessage);
  }
}

// Parses the command line for Google Test flags, without initializing
// other parts of Google Test. This function updates argc and argv by removing
// flags that are known to GoogleTest (including other user flags defined using
// ABSL_FLAG if GoogleTest is built with GTEST_USE_ABSL). Other arguments
// remain in place. Unrecognized flags are not reported and do not cause the
// program to exit.
void ParseGoogleTestFlagsOnly(int* argc, char** argv) {
#ifdef GTEST_HAS_ABSL_FLAGS
  if (*argc <= 0) return;

  std::vector<char*> positional_args;
  std::vector<absl::UnrecognizedFlag> unrecognized_flags;
  absl::ParseAbseilFlagsOnly(*argc, argv, positional_args, unrecognized_flags);
  absl::flat_hash_set<absl::string_view> unrecognized;
  for (const auto& flag : unrecognized_flags) {
    unrecognized.insert(flag.flag_name);
  }
  absl::flat_hash_set<char*> positional;
  for (const auto& arg : positional_args) {
    positional.insert(arg);
  }

  int out_pos = 1;
  int in_pos = 1;
  for (; in_pos < *argc; ++in_pos) {
    char* arg = argv[in_pos];
    absl::string_view arg_str(arg);
    if (absl::ConsumePrefix(&arg_str, "--")) {
      // Flag-like argument. If the flag was unrecognized, keep it.
      // If it was a GoogleTest flag, remove it.
      if (unrecognized.contains(arg_str)) {
        argv[out_pos++] = argv[in_pos];
        continue;
      }
    }

    if (arg_str.empty()) {
      ++in_pos;
      break;  // '--' indicates that the rest of the arguments are positional
    }

    // Probably a positional argument. If it is in fact positional, keep it.
    // If it was a value for the flag argument, remove it.
    if (positional.contains(arg)) {
      argv[out_pos++] = arg;
    }
  }

  // The rest are positional args for sure.
  while (in_pos < *argc) {
    argv[out_pos++] = argv[in_pos++];
  }

  *argc = out_pos;
  argv[out_pos] = nullptr;
#else
  ParseGoogleTestFlagsOnlyImpl(argc, argv);
#endif

  // Fix the value of *_NSGetArgc() on macOS, but if and only if
  // *_NSGetArgv() == argv
  // Only applicable to char** version of argv
#ifdef GTEST_OS_MAC
#ifndef GTEST_OS_IOS
  if (*_NSGetArgv() == argv) {
    *_NSGetArgc() = *argc;
  }
#endif
#endif
}
void ParseGoogleTestFlagsOnly(int* argc, wchar_t** argv) {
  ParseGoogleTestFlagsOnlyImpl(argc, argv);
}

// The internal implementation of InitGoogleTest().
//
// The type parameter CharType can be instantiated to either char or
// wchar_t.
template <typename CharType>
void InitGoogleTestImpl(int* argc, CharType** argv) {
  // We don't want to run the initialization code twice.
  if (GTestIsInitialized()) return;

  if (*argc <= 0) return;

  g_argvs.clear();
  for (int i = 0; i != *argc; i++) {
    g_argvs.push_back(StreamableToString(argv[i]));
  }

#ifdef GTEST_HAS_ABSL
  absl::InitializeSymbolizer(g_argvs[0].c_str());

#ifdef GTEST_HAS_ABSL_FLAGS
  // When using the Abseil Flags library, set the program usage message to the
  // help message, but remove the color-encoding from the message first.
  absl::SetProgramUsageMessage(absl::StrReplaceAll(
      kColorEncodedHelpMessage,
      {{"@D", ""}, {"@R", ""}, {"@G", ""}, {"@Y", ""}, {"@@", "@"}}));
#endif  // GTEST_HAS_ABSL_FLAGS
#endif  // GTEST_HAS_ABSL

  ParseGoogleTestFlagsOnly(argc, argv);
  GetUnitTestImpl()->PostFlagParsingInit();
}

}  // namespace internal

// Initializes Google Test.  This must be called before calling
// RUN_ALL_TESTS().  In particular, it parses a command line for the
// flags that Google Test recognizes.  Whenever a Google Test flag is
// seen, it is removed from argv, and *argc is decremented.
//
// No value is returned.  Instead, the Google Test flag variables are
// updated.
//
// Calling the function for the second time has no user-visible effect.
void InitGoogleTest(int* argc, char** argv) {
#if defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_(argc, argv);
#else   // defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  internal::InitGoogleTestImpl(argc, argv);
#endif  // defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
}

// This overloaded version can be used in Windows programs compiled in
// UNICODE mode.
void InitGoogleTest(int* argc, wchar_t** argv) {
#if defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_(argc, argv);
#else   // defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  internal::InitGoogleTestImpl(argc, argv);
#endif  // defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
}

// This overloaded version can be used on Arduino/embedded platforms where
// there is no argc/argv.
void InitGoogleTest() {
  // Since Arduino doesn't have a command line, fake out the argc/argv arguments
  int argc = 1;
  const auto arg0 = "dummy";
  char* argv0 = const_cast<char*>(arg0);
  char** argv = &argv0;

#if defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_(&argc, argv);
#else   // defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  internal::InitGoogleTestImpl(&argc, argv);
#endif  // defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
}

#if !defined(GTEST_CUSTOM_TEMPDIR_FUNCTION_) || \
    !defined(GTEST_CUSTOM_SRCDIR_FUNCTION_)
// Returns the value of the first environment variable that is set and contains
// a non-empty string. If there are none, returns the "fallback" string. Adds
// the director-separator character as a suffix if not provided in the
// environment variable value.
static std::string GetDirFromEnv(
    std::initializer_list<const char*> environment_variables,
    const char* fallback, char separator) {
  for (const char* variable_name : environment_variables) {
    const char* value = internal::posix::GetEnv(variable_name);
    if (value != nullptr && value[0] != '\0') {
      if (value[strlen(value) - 1] != separator) {
        return std::string(value).append(1, separator);
      }
      return value;
    }
  }
  return fallback;
}
#endif

std::string TempDir() {
#if defined(GTEST_CUSTOM_TEMPDIR_FUNCTION_)
  return GTEST_CUSTOM_TEMPDIR_FUNCTION_();
#elif defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_WINDOWS_MOBILE)
  return GetDirFromEnv({"TEST_TMPDIR", "TEMP"}, "\\temp\\", '\\');
#elif defined(GTEST_OS_LINUX_ANDROID)
  return GetDirFromEnv({"TEST_TMPDIR", "TMPDIR"}, "/data/local/tmp/", '/');
#else
  return GetDirFromEnv({"TEST_TMPDIR", "TMPDIR"}, "/tmp/", '/');
#endif
}

#if GTEST_HAS_FILE_SYSTEM && !defined(GTEST_CUSTOM_SRCDIR_FUNCTION_)
// Returns the directory path (including terminating separator) of the current
// executable as derived from argv[0].
static std::string GetCurrentExecutableDirectory() {
  internal::FilePath argv_0(internal::GetArgvs()[0]);
  return argv_0.RemoveFileName().string();
}
#endif

#if GTEST_HAS_FILE_SYSTEM
std::string SrcDir() {
#if defined(GTEST_CUSTOM_SRCDIR_FUNCTION_)
  return GTEST_CUSTOM_SRCDIR_FUNCTION_();
#elif defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_WINDOWS_MOBILE)
  return GetDirFromEnv({"TEST_SRCDIR"}, GetCurrentExecutableDirectory().c_str(),
                       '\\');
#elif defined(GTEST_OS_LINUX_ANDROID)
  return GetDirFromEnv({"TEST_SRCDIR"}, GetCurrentExecutableDirectory().c_str(),
                       '/');
#else
  return GetDirFromEnv({"TEST_SRCDIR"}, GetCurrentExecutableDirectory().c_str(),
                       '/');
#endif
}
#endif

// Class ScopedTrace

// Pushes the given source file location and message onto a per-thread
// trace stack maintained by Google Test.
void ScopedTrace::PushTrace(const char* file, int line, std::string message) {
  internal::TraceInfo trace;
  trace.file = file;
  trace.line = line;
  trace.message.swap(message);

  UnitTest::GetInstance()->PushGTestTrace(trace);
}

// Pops the info pushed by the c'tor.
ScopedTrace::~ScopedTrace() GTEST_LOCK_EXCLUDED_(&UnitTest::mutex_) {
  UnitTest::GetInstance()->PopGTestTrace();
}

}  // namespace testing
