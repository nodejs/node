// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "src/base/logging.h"
#include "src/torque/ast.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

std::string StringLiteralUnquote(const std::string& s) {
  DCHECK(('"' == s.front() && '"' == s.back()) ||
         ('\'' == s.front() && '\'' == s.back()));
  std::stringstream result;
  for (size_t i = 1; i < s.length() - 1; ++i) {
    if (s[i] == '\\') {
      switch (s[++i]) {
        case 'n':
          result << '\n';
          break;
        case 'r':
          result << '\r';
          break;
        case 't':
          result << '\t';
          break;
        case '\'':
        case '"':
        case '\\':
          result << s[i];
          break;
        default:
          UNREACHABLE();
      }
    } else {
      result << s[i];
    }
  }
  return result.str();
}

std::string StringLiteralQuote(const std::string& s) {
  std::stringstream result;
  result << '"';
  for (size_t i = 0; i < s.length() - 1; ++i) {
    switch (s[i]) {
      case '\n':
        result << "\\n";
        break;
      case '\r':
        result << "\\r";
        break;
      case '\t':
        result << "\\t";
        break;
      case '\'':
      case '"':
      case '\\':
        result << "\\" << s[i];
        break;
      default:
        result << s[i];
    }
  }
  result << '"';
  return result.str();
}

std::string CurrentPositionAsString() {
  return PositionAsString(CurrentSourcePosition::Get());
}

DEFINE_CONTEXTUAL_VARIABLE(LintErrorStatus)

[[noreturn]] void ReportErrorString(const std::string& error) {
  std::cerr << CurrentPositionAsString() << ": Torque error: " << error << "\n";
  v8::base::OS::Abort();
}

void LintError(const std::string& error) {
  LintErrorStatus::SetLintError();
  std::cerr << CurrentPositionAsString() << ": Lint error: " << error << "\n";
}

void NamingConventionError(const std::string& type, const std::string& name,
                           const std::string& convention) {
  std::stringstream sstream;
  sstream << type << " \"" << name << "\" doesn't follow \"" << convention
          << "\" naming convention.";
  LintError(sstream.str());
}

namespace {

bool ContainsUnderscore(const std::string& s) {
  if (s.empty()) return false;
  return s.find("_") != std::string::npos;
}

bool ContainsUpperCase(const std::string& s) {
  if (s.empty()) return false;
  return std::any_of(s.begin(), s.end(), [](char c) { return isupper(c); });
}

// Torque has some module constants that are used like language level
// keywords, e.g.: 'True', 'Undefined', etc.
// These do not need to follow the default naming convention for constants.
bool IsKeywordLikeName(const std::string& s) {
  static const std::vector<std::string> keyword_like_constants{
      "True", "False", "Hole", "Null", "Undefined"};

  return std::find(keyword_like_constants.begin(), keyword_like_constants.end(),
                   s) != keyword_like_constants.end();
}

// Untagged/MachineTypes like 'int32', 'intptr' etc. follow a 'all-lowercase'
// naming convention and are those exempt from the normal type convention.
bool IsMachineType(const std::string& s) {
  static const std::vector<std::string> machine_types{
      "void",    "never",   "int32",   "uint32", "int64",  "intptr",
      "uintptr", "float32", "float64", "bool",   "string", "int31"};

  return std::find(machine_types.begin(), machine_types.end(), s) !=
         machine_types.end();
}

}  // namespace

bool IsLowerCamelCase(const std::string& s) {
  if (s.empty()) return false;
  return islower(s[0]) && !ContainsUnderscore(s);
}

bool IsUpperCamelCase(const std::string& s) {
  if (s.empty()) return false;
  return isupper(s[0]) && !ContainsUnderscore(s);
}

bool IsSnakeCase(const std::string& s) {
  if (s.empty()) return false;
  return !ContainsUpperCase(s);
}

bool IsValidModuleConstName(const std::string& s) {
  if (s.empty()) return false;
  if (IsKeywordLikeName(s)) return true;

  return s[0] == 'k' && IsUpperCamelCase(s.substr(1));
}

bool IsValidTypeName(const std::string& s) {
  if (s.empty()) return false;
  if (IsMachineType(s)) return true;

  return IsUpperCamelCase(s);
}

std::string CamelifyString(const std::string& underscore_string) {
  std::string result;
  bool word_beginning = true;
  for (auto current : underscore_string) {
    if (current == '_' || current == '-') {
      word_beginning = true;
      continue;
    }
    if (word_beginning) {
      current = toupper(current);
    }
    result += current;
    word_beginning = false;
  }
  return result;
}

std::string DashifyString(const std::string& underscore_string) {
  std::string result = underscore_string;
  std::replace(result.begin(), result.end(), '_', '-');
  return result;
}

void ReplaceFileContentsIfDifferent(const std::string& file_path,
                                    const std::string& contents) {
  std::ifstream old_contents_stream(file_path.c_str());
  std::string old_contents;
  if (old_contents_stream.good()) {
    std::istreambuf_iterator<char> eos;
    old_contents =
        std::string(std::istreambuf_iterator<char>(old_contents_stream), eos);
    old_contents_stream.close();
  }
  if (old_contents.length() == 0 || old_contents != contents) {
    std::ofstream new_contents_stream;
    new_contents_stream.open(file_path.c_str());
    new_contents_stream << contents;
    new_contents_stream.close();
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
