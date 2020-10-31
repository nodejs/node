// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#define _CRT_SECURE_NO_WARNINGS 1 // Prevents deprecation warning with MSVC
#include "jsontest.h"
#include <cstdio>
#include <string>

#if defined(_MSC_VER)
// Used to install a report hook that prevent dialog on assertion and error.
#include <crtdbg.h>
#endif // if defined(_MSC_VER)

#if defined(_WIN32)
// Used to prevent dialog on memory fault.
// Limits headers included by Windows.h
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOSOUND
#define NOCOMM
#define NORPC
#define NOGDI
#define NOUSER
#define NODRIVERS
#define NOLOGERROR
#define NOPROFILER
#define NOMEMMGR
#define NOLFILEIO
#define NOOPENFILE
#define NORESOURCE
#define NOATOM
#define NOLANGUAGE
#define NOLSTRING
#define NODBCS
#define NOKEYBOARDINFO
#define NOGDICAPMASKS
#define NOCOLOR
#define NOGDIOBJ
#define NODRAWTEXT
#define NOTEXTMETRIC
#define NOSCALABLEFONT
#define NOBITMAP
#define NORASTEROPS
#define NOMETAFILE
#define NOSYSMETRICS
#define NOSYSTEMPARAMSINFO
#define NOMSG
#define NOWINSTYLES
#define NOWINOFFSETS
#define NOSHOWWINDOW
#define NODEFERWINDOWPOS
#define NOVIRTUALKEYCODES
#define NOKEYSTATES
#define NOWH
#define NOMENUS
#define NOSCROLL
#define NOCLIPBOARD
#define NOICONS
#define NOMB
#define NOSYSCOMMANDS
#define NOMDI
#define NOCTLMGR
#define NOWINMESSAGES
#include <windows.h>
#endif // if defined(_WIN32)

namespace JsonTest {

// class TestResult
// //////////////////////////////////////////////////////////////////

TestResult::TestResult() {
  // The root predicate has id 0
  rootPredicateNode_.id_ = 0;
  rootPredicateNode_.next_ = nullptr;
  predicateStackTail_ = &rootPredicateNode_;
}

void TestResult::setTestName(const Json::String& name) { name_ = name; }

TestResult& TestResult::addFailure(const char* file, unsigned int line,
                                   const char* expr) {
  /// Walks the PredicateContext stack adding them to failures_ if not already
  /// added.
  unsigned int nestingLevel = 0;
  PredicateContext* lastNode = rootPredicateNode_.next_;
  for (; lastNode != nullptr; lastNode = lastNode->next_) {
    if (lastNode->id_ > lastUsedPredicateId_) // new PredicateContext
    {
      lastUsedPredicateId_ = lastNode->id_;
      addFailureInfo(lastNode->file_, lastNode->line_, lastNode->expr_,
                     nestingLevel);
      // Link the PredicateContext to the failure for message target when
      // popping the PredicateContext.
      lastNode->failure_ = &(failures_.back());
    }
    ++nestingLevel;
  }

  // Adds the failed assertion
  addFailureInfo(file, line, expr, nestingLevel);
  messageTarget_ = &(failures_.back());
  return *this;
}

void TestResult::addFailureInfo(const char* file, unsigned int line,
                                const char* expr, unsigned int nestingLevel) {
  Failure failure;
  failure.file_ = file;
  failure.line_ = line;
  if (expr) {
    failure.expr_ = expr;
  }
  failure.nestingLevel_ = nestingLevel;
  failures_.push_back(failure);
}

TestResult& TestResult::popPredicateContext() {
  PredicateContext* lastNode = &rootPredicateNode_;
  while (lastNode->next_ != nullptr && lastNode->next_->next_ != nullptr) {
    lastNode = lastNode->next_;
  }
  // Set message target to popped failure
  PredicateContext* tail = lastNode->next_;
  if (tail != nullptr && tail->failure_ != nullptr) {
    messageTarget_ = tail->failure_;
  }
  // Remove tail from list
  predicateStackTail_ = lastNode;
  lastNode->next_ = nullptr;
  return *this;
}

bool TestResult::failed() const { return !failures_.empty(); }

void TestResult::printFailure(bool printTestName) const {
  if (failures_.empty()) {
    return;
  }

  if (printTestName) {
    printf("* Detail of %s test failure:\n", name_.c_str());
  }

  // Print in reverse to display the callstack in the right order
  for (const auto& failure : failures_) {
    Json::String indent(failure.nestingLevel_ * 2, ' ');
    if (failure.file_) {
      printf("%s%s(%u): ", indent.c_str(), failure.file_, failure.line_);
    }
    if (!failure.expr_.empty()) {
      printf("%s\n", failure.expr_.c_str());
    } else if (failure.file_) {
      printf("\n");
    }
    if (!failure.message_.empty()) {
      Json::String reindented = indentText(failure.message_, indent + "  ");
      printf("%s\n", reindented.c_str());
    }
  }
}

Json::String TestResult::indentText(const Json::String& text,
                                    const Json::String& indent) {
  Json::String reindented;
  Json::String::size_type lastIndex = 0;
  while (lastIndex < text.size()) {
    Json::String::size_type nextIndex = text.find('\n', lastIndex);
    if (nextIndex == Json::String::npos) {
      nextIndex = text.size() - 1;
    }
    reindented += indent;
    reindented += text.substr(lastIndex, nextIndex - lastIndex + 1);
    lastIndex = nextIndex + 1;
  }
  return reindented;
}

TestResult& TestResult::addToLastFailure(const Json::String& message) {
  if (messageTarget_ != nullptr) {
    messageTarget_->message_ += message;
  }
  return *this;
}

TestResult& TestResult::operator<<(Json::Int64 value) {
  return addToLastFailure(Json::valueToString(value));
}

TestResult& TestResult::operator<<(Json::UInt64 value) {
  return addToLastFailure(Json::valueToString(value));
}

TestResult& TestResult::operator<<(bool value) {
  return addToLastFailure(value ? "true" : "false");
}

// class TestCase
// //////////////////////////////////////////////////////////////////

TestCase::TestCase() = default;

TestCase::~TestCase() = default;

void TestCase::run(TestResult& result) {
  result_ = &result;
  runTestCase();
}

// class Runner
// //////////////////////////////////////////////////////////////////

Runner::Runner() = default;

Runner& Runner::add(TestCaseFactory factory) {
  tests_.push_back(factory);
  return *this;
}

size_t Runner::testCount() const { return tests_.size(); }

Json::String Runner::testNameAt(size_t index) const {
  TestCase* test = tests_[index]();
  Json::String name = test->testName();
  delete test;
  return name;
}

void Runner::runTestAt(size_t index, TestResult& result) const {
  TestCase* test = tests_[index]();
  result.setTestName(test->testName());
  printf("Testing %s: ", test->testName());
  fflush(stdout);
#if JSON_USE_EXCEPTION
  try {
#endif // if JSON_USE_EXCEPTION
    test->run(result);
#if JSON_USE_EXCEPTION
  } catch (const std::exception& e) {
    result.addFailure(__FILE__, __LINE__, "Unexpected exception caught:")
        << e.what();
  }
#endif // if JSON_USE_EXCEPTION
  delete test;
  const char* status = result.failed() ? "FAILED" : "OK";
  printf("%s\n", status);
  fflush(stdout);
}

bool Runner::runAllTest(bool printSummary) const {
  size_t const count = testCount();
  std::deque<TestResult> failures;
  for (size_t index = 0; index < count; ++index) {
    TestResult result;
    runTestAt(index, result);
    if (result.failed()) {
      failures.push_back(result);
    }
  }

  if (failures.empty()) {
    if (printSummary) {
      printf("All %zu tests passed\n", count);
    }
    return true;
  }
  for (auto& result : failures) {
    result.printFailure(count > 1);
  }

  if (printSummary) {
    size_t const failedCount = failures.size();
    size_t const passedCount = count - failedCount;
    printf("%zu/%zu tests passed (%zu failure(s))\n", passedCount, count,
           failedCount);
  }
  return false;
}

bool Runner::testIndex(const Json::String& testName, size_t& indexOut) const {
  const size_t count = testCount();
  for (size_t index = 0; index < count; ++index) {
    if (testNameAt(index) == testName) {
      indexOut = index;
      return true;
    }
  }
  return false;
}

void Runner::listTests() const {
  const size_t count = testCount();
  for (size_t index = 0; index < count; ++index) {
    printf("%s\n", testNameAt(index).c_str());
  }
}

int Runner::runCommandLine(int argc, const char* argv[]) const {
  // typedef std::deque<String> TestNames;
  Runner subrunner;
  for (int index = 1; index < argc; ++index) {
    Json::String opt = argv[index];
    if (opt == "--list-tests") {
      listTests();
      return 0;
    }
    if (opt == "--test-auto") {
      preventDialogOnCrash();
    } else if (opt == "--test") {
      ++index;
      if (index < argc) {
        size_t testNameIndex;
        if (testIndex(argv[index], testNameIndex)) {
          subrunner.add(tests_[testNameIndex]);
        } else {
          fprintf(stderr, "Test '%s' does not exist!\n", argv[index]);
          return 2;
        }
      } else {
        printUsage(argv[0]);
        return 2;
      }
    } else {
      printUsage(argv[0]);
      return 2;
    }
  }
  bool succeeded;
  if (subrunner.testCount() > 0) {
    succeeded = subrunner.runAllTest(subrunner.testCount() > 1);
  } else {
    succeeded = runAllTest(true);
  }
  return succeeded ? 0 : 1;
}

#if defined(_MSC_VER) && defined(_DEBUG)
// Hook MSVCRT assertions to prevent dialog from appearing
static int msvcrtSilentReportHook(int reportType, char* message,
                                  int* /*returnValue*/) {
  // The default CRT handling of error and assertion is to display
  // an error dialog to the user.
  // Instead, when an error or an assertion occurs, we force the
  // application to terminate using abort() after display
  // the message on stderr.
  if (reportType == _CRT_ERROR || reportType == _CRT_ASSERT) {
    // calling abort() cause the ReportHook to be called
    // The following is used to detect this case and let's the
    // error handler fallback on its default behaviour (
    // display a warning message)
    static volatile bool isAborting = false;
    if (isAborting) {
      return TRUE;
    }
    isAborting = true;

    fprintf(stderr, "CRT Error/Assert:\n%s\n", message);
    fflush(stderr);
    abort();
  }
  // Let's other reportType (_CRT_WARNING) be handled as they would by default
  return FALSE;
}
#endif // if defined(_MSC_VER)

void Runner::preventDialogOnCrash() {
#if defined(_MSC_VER) && defined(_DEBUG)
  // Install a hook to prevent MSVCRT error and assertion from
  // popping a dialog
  // This function a NO-OP in release configuration
  // (which cause warning since msvcrtSilentReportHook is not referenced)
  _CrtSetReportHook(&msvcrtSilentReportHook);
#endif // if defined(_MSC_VER)

  // @todo investigate this handler (for buffer overflow)
  // _set_security_error_handler

#if defined(_WIN32)
  // Prevents the system from popping a dialog for debugging if the
  // application fails due to invalid memory access.
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);
#endif // if defined(_WIN32)
}

void Runner::printUsage(const char* appName) {
  printf("Usage: %s [options]\n"
         "\n"
         "If --test is not specified, then all the test cases be run.\n"
         "\n"
         "Valid options:\n"
         "--list-tests: print the name of all test cases on the standard\n"
         "              output and exit.\n"
         "--test TESTNAME: executes the test case with the specified name.\n"
         "                 May be repeated.\n"
         "--test-auto: prevent dialog prompting for debugging on crash.\n",
         appName);
}

// Assertion functions
// //////////////////////////////////////////////////////////////////

Json::String ToJsonString(const char* toConvert) {
  return Json::String(toConvert);
}

Json::String ToJsonString(Json::String in) { return in; }

#if JSONCPP_USING_SECURE_MEMORY
Json::String ToJsonString(std::string in) {
  return Json::String(in.data(), in.data() + in.length());
}
#endif

TestResult& checkStringEqual(TestResult& result, const Json::String& expected,
                             const Json::String& actual, const char* file,
                             unsigned int line, const char* expr) {
  if (expected != actual) {
    result.addFailure(file, line, expr);
    result << "Expected: '" << expected << "'\n";
    result << "Actual  : '" << actual << "'";
  }
  return result;
}

} // namespace JsonTest
