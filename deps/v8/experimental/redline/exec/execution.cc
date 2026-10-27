// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/exec/execution.h"

#include <cctype>
#include <string_view>

namespace redline {

const char* OutcomeName(Outcome outcome) {
  switch (outcome) {
    case Outcome::kSucceeded:
      return "succeeded";
    case Outcome::kFailed:
      return "failed";
    case Outcome::kCrashed:
      return "crashed";
    case Outcome::kTimedOut:
      return "timed_out";
  }
  return "unknown";
}

namespace {

// Finds "<Name>Error: ..." in `line` (d8 prints "file:line: TypeError: msg"
// first, then the source line, then the bare "TypeError: msg" + stack).
std::string_view ErrorIn(std::string_view line) {
  size_t pos = line.find("Error: ");
  if (pos == std::string_view::npos) {
    return {};
  }
  size_t begin = pos;
  while (begin > 0 &&
         (std::isalnum(static_cast<unsigned char>(line[begin - 1])) ||
          line[begin - 1] == '_' || line[begin - 1] == '$')) {
    begin--;
  }
  return line.substr(begin);
}

}  // namespace

std::string Execution::ExceptionSignature() const {
  for (const std::string* output : {&stdout_output, &stderr_output}) {
    std::string_view rest = *output;
    while (!rest.empty()) {
      size_t newline = rest.find('\n');
      std::string_view line = rest.substr(0, newline);
      std::string_view error = ErrorIn(line);
      if (!error.empty()) {
        return std::string(error);
      }
      if (newline == std::string_view::npos) {
        break;
      }
      rest.remove_prefix(newline + 1);
    }
  }
  return std::string();
}

bool Execution::IsParseError() const {
  if (outcome != Outcome::kFailed) {
    return false;
  }
  std::string signature = ExceptionSignature();
  if (!std::string_view(signature).starts_with("SyntaxError")) {
    return false;
  }
  for (const std::string* output : {&stdout_output, &stderr_output}) {
    if (output->find("\n    at ") != std::string::npos) {
      return false;
    }
  }
  return true;
}

}  // namespace redline
