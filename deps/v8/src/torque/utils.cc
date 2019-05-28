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
  for (size_t i = 0; i < s.length(); ++i) {
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

static const char kFileUriPrefix[] = "file://";
static const int kFileUriPrefixLength = sizeof(kFileUriPrefix) - 1;

static int HexCharToInt(unsigned char c) {
  if (isdigit(c)) return c - '0';
  if (isupper(c)) return c - 'A' + 10;
  DCHECK(islower(c));
  return c - 'a' + 10;
}

base::Optional<std::string> FileUriDecode(const std::string& uri) {
  // Abort decoding of URIs that don't start with "file://".
  if (uri.rfind(kFileUriPrefix) != 0) return base::nullopt;

  const std::string path = uri.substr(kFileUriPrefixLength);
  std::ostringstream decoded;

  for (auto iter = path.begin(), end = path.end(); iter != end; ++iter) {
    std::string::value_type c = (*iter);

    // Normal characters are appended.
    if (c != '%') {
      decoded << c;
      continue;
    }

    // If '%' is not followed by at least two hex digits, we abort.
    if (std::distance(iter, end) <= 2) return base::nullopt;

    unsigned char first = (*++iter);
    unsigned char second = (*++iter);
    if (!isxdigit(first) || !isxdigit(second)) return base::nullopt;

    // An escaped hex value needs converting.
    unsigned char value = HexCharToInt(first) * 16 + HexCharToInt(second);
    decoded << value;
  }

  return decoded.str();
}

std::string CurrentPositionAsString() {
  return PositionAsString(CurrentSourcePosition::Get());
}

DEFINE_CONTEXTUAL_VARIABLE(LintErrorStatus)

[[noreturn]] void ThrowTorqueError(const std::string& message,
                                   bool include_position) {
  TorqueError error(message);
  if (include_position) error.position = CurrentSourcePosition::Get();
  throw error;
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

// Torque has some namespace constants that are used like language level
// keywords, e.g.: 'True', 'Undefined', etc.
// These do not need to follow the default naming convention for constants.
bool IsKeywordLikeName(const std::string& s) {
  static const char* const keyword_like_constants[]{"True", "False", "Hole",
                                                    "Null", "Undefined"};

  return std::find(std::begin(keyword_like_constants),
                   std::end(keyword_like_constants),
                   s) != std::end(keyword_like_constants);
}

// Untagged/MachineTypes like 'int32', 'intptr' etc. follow a 'all-lowercase'
// naming convention and are those exempt from the normal type convention.
bool IsMachineType(const std::string& s) {
  static const char* const machine_types[]{
      "void",    "never",   "int8",    "uint8",  "int16",  "uint16",
      "int31",   "uint31",  "int32",   "uint32", "int64",  "intptr",
      "uintptr", "float32", "float64", "bool",   "string", "bint"};

  return std::find(std::begin(machine_types), std::end(machine_types), s) !=
         std::end(machine_types);
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

bool IsValidNamespaceConstName(const std::string& s) {
  if (s.empty()) return false;
  if (IsKeywordLikeName(s)) return true;

  return s[0] == 'k' && IsUpperCamelCase(s.substr(1));
}

bool IsValidTypeName(const std::string& s) {
  if (s.empty()) return false;
  if (IsMachineType(s)) return true;

  return IsUpperCamelCase(s);
}

std::string CapifyStringWithUnderscores(const std::string& camellified_string) {
  std::string result;
  bool previousWasLower = false;
  for (auto current : camellified_string) {
    if (previousWasLower && isupper(current)) {
      result += "_";
    }
    result += toupper(current);
    previousWasLower = (islower(current));
  }
  return result;
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
