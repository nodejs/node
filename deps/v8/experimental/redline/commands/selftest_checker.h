// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_COMMANDS_SELFTEST_CHECKER_H_
#define V8_EXPERIMENTAL_REDLINE_COMMANDS_SELFTEST_CHECKER_H_

#include <cstdio>
#include <string>

namespace redline {

// Counts and prints the PASS/FAIL lines of `redline selftest`. Shared by the
// target checks (selftest_command.cc) and the offline checks
// (selftest_selection.cc).
class Checker {
 public:
  bool Check(const char* name, bool ok, const std::string& details = "") {
    printf("%s  %s%s%s\n", ok ? "PASS" : "FAIL", name,
           details.empty() ? "" : ": ", details.c_str());
    fflush(stdout);
    if (ok) {
      passed_++;
    } else {
      failed_++;
    }
    return ok;
  }

  void Info(const char* name, const std::string& details) {
    printf("INFO  %s: %s\n", name, details.c_str());
    fflush(stdout);
  }

  int passed() const { return passed_; }
  int failed() const { return failed_; }

 private:
  int passed_ = 0;
  int failed_ = 0;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_COMMANDS_SELFTEST_CHECKER_H_
