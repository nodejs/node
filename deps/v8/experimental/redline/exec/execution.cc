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

constexpr std::string_view kFuzzcodePrefix = "fuzzcode.js:";
constexpr int kMaxLineNumber = 1'000'000'000;

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

// Parses a positive decimal integer starting at `pos` in `text`.
bool ParsePositiveInt(std::string_view text, size_t pos, int* out,
                      size_t* end_pos) {
  if (pos >= text.size() ||
      !std::isdigit(static_cast<unsigned char>(text[pos]))) {
    return false;
  }
  int64_t value = 0;
  size_t i = pos;
  while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
    value = value * 10 + (text[i] - '0');
    if (value > kMaxLineNumber) {
      return false;
    }
    i++;
  }
  if (value <= 0) {
    return false;
  }
  *out = static_cast<int>(value);
  *end_pos = i;
  return true;
}

// Matches a `d8` exception header line of the form `fuzzcode.js:<line>: ...`.
bool ParseFuzzcodeHeaderLine(std::string_view line, int* raw_line) {
  if (!line.starts_with(kFuzzcodePrefix)) {
    return false;
  }
  int parsed = 0;
  size_t after = 0;
  if (!ParsePositiveInt(line, kFuzzcodePrefix.size(), &parsed, &after)) {
    return false;
  }
  if (after >= line.size() || line[after] != ':') {
    return false;
  }
  *raw_line = parsed;
  return true;
}

// Matches a JS stack frame line that references `fuzzcode.js:<line>:<col>`.
bool ParseFuzzcodeStackLine(std::string_view line, int* raw_line) {
  size_t search = 0;
  while (search < line.size()) {
    size_t pos = line.find(kFuzzcodePrefix, search);
    if (pos == std::string_view::npos) {
      return false;
    }
    search = pos + kFuzzcodePrefix.size();
    if (pos == 0 || (line[pos - 1] != ' ' && line[pos - 1] != '(')) {
      continue;
    }
    int line_num = 0;
    size_t after_line = 0;
    if (!ParsePositiveInt(line, search, &line_num, &after_line)) {
      continue;
    }
    if (after_line >= line.size() || line[after_line] != ':') {
      continue;
    }
    int col_num = 0;
    size_t after_col = 0;
    if (!ParsePositiveInt(line, after_line + 1, &col_num, &after_col)) {
      continue;
    }
    if (after_col == line.size() || line[after_col] == ')') {
      *raw_line = line_num;
      return true;
    }
  }
  return false;
}

// Scans `text` for the last uncaught `fuzzcode.js` exception header and stack.
ExceptionLines ScanExceptionOutput(std::string_view text, int prelude_lines) {
  bool in_stack = false;
  int header_line = 0;
  int frame_inner = 0;
  int frame_outer = 0;
  std::string_view rest = text;
  while (!rest.empty()) {
    size_t newline = rest.find('\n');
    std::string_view line = rest.substr(0, newline);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    int raw_line = 0;
    if (line.starts_with("    at ")) {
      if (!in_stack) {
        in_stack = true;
        frame_inner = 0;
        frame_outer = 0;
      }
      if (ParseFuzzcodeStackLine(line, &raw_line) && raw_line > prelude_lines) {
        int program_line = raw_line - prelude_lines;
        if (frame_inner == 0) {
          frame_inner = program_line;
        }
        frame_outer = program_line;
      }
    } else {
      in_stack = false;
      if (ParseFuzzcodeHeaderLine(line, &raw_line)) {
        frame_inner = 0;
        frame_outer = 0;
        header_line = raw_line > prelude_lines ? raw_line - prelude_lines : 0;
      }
    }
    if (newline == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(newline + 1);
  }
  if (frame_inner > 0) {
    return {frame_inner, frame_outer};
  }
  if (header_line > 0) {
    return {header_line, header_line};
  }
  return {};
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

ExceptionLines ExtractExceptionLines(const Execution& execution,
                                     int prelude_lines) {
  if (execution.outcome != Outcome::kFailed || execution.IsParseError()) {
    return {};
  }
  int offset = prelude_lines > 0 ? prelude_lines : 0;
  ExceptionLines lines = ScanExceptionOutput(execution.stdout_output, offset);
  if (!lines.empty()) {
    return lines;
  }
  return ScanExceptionOutput(execution.stderr_output, offset);
}

}  // namespace redline
