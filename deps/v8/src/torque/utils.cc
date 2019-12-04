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

DEFINE_CONTEXTUAL_VARIABLE(TorqueMessages)

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

#ifdef V8_OS_WIN
static const char kFileUriPrefix[] = "file:///";
#else
static const char kFileUriPrefix[] = "file://";
#endif
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

MessageBuilder::MessageBuilder(const std::string& message,
                               TorqueMessage::Kind kind) {
  base::Optional<SourcePosition> position;
  if (CurrentSourcePosition::HasScope()) {
    position = CurrentSourcePosition::Get();
  }
  message_ = TorqueMessage{message, position, kind};
}

void MessageBuilder::Report() const {
  TorqueMessages::Get().push_back(message_);
}

[[noreturn]] void MessageBuilder::Throw() const {
  throw TorqueAbortCompilation{};
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
  static const char* const keyword_like_constants[]{"True", "False", "TheHole",
                                                    "Null", "Undefined"};

  return std::find(std::begin(keyword_like_constants),
                   std::end(keyword_like_constants),
                   s) != std::end(keyword_like_constants);
}

// Untagged/MachineTypes like 'int32', 'intptr' etc. follow a 'all-lowercase'
// naming convention and are those exempt from the normal type convention.
bool IsMachineType(const std::string& s) {
  static const char* const machine_types[]{
      "void",    "never", "int8",   "uint8", "int16",  "uint16",  "int31",
      "uint31",  "int32", "uint32", "int64", "intptr", "uintptr", "float32",
      "float64", "bool",  "string", "bint",  "char8",  "char16"};

  return std::find(std::begin(machine_types), std::end(machine_types), s) !=
         std::end(machine_types);
}

}  // namespace

bool IsLowerCamelCase(const std::string& s) {
  if (s.empty()) return false;
  size_t start = 0;
  if (s[0] == '_') start = 1;
  return islower(s[start]) && !ContainsUnderscore(s.substr(start));
}

bool IsUpperCamelCase(const std::string& s) {
  if (s.empty()) return false;
  size_t start = 0;
  if (s[0] == '_') start = 1;
  return isupper(s[start]) && !ContainsUnderscore(s.substr(1));
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
    if (current == '.' || current == '-') {
      result += "_";
      previousWasLower = false;
      continue;
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

std::string SnakeifyString(const std::string& camel_string) {
  std::string result;
  bool previousWasLower = false;
  for (auto current : camel_string) {
    if (previousWasLower && isupper(current)) {
      result += "_";
    }
    result += tolower(current);
    previousWasLower = (islower(current));
  }
  return result;
}

std::string DashifyString(const std::string& underscore_string) {
  std::string result = underscore_string;
  std::replace(result.begin(), result.end(), '_', '-');
  return result;
}

std::string UnderlinifyPath(std::string path) {
  std::replace(path.begin(), path.end(), '-', '_');
  std::replace(path.begin(), path.end(), '/', '_');
  std::replace(path.begin(), path.end(), '\\', '_');
  std::replace(path.begin(), path.end(), '.', '_');
  transform(path.begin(), path.end(), path.begin(), ::toupper);
  return path;
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

IfDefScope::IfDefScope(std::ostream& os, std::string d)
    : os_(os), d_(std::move(d)) {
  os_ << "#ifdef " << d_ << "\n";
}
IfDefScope::~IfDefScope() { os_ << "#endif  // " << d_ << "\n"; }

NamespaceScope::NamespaceScope(std::ostream& os,
                               std::initializer_list<std::string> namespaces)
    : os_(os), d_(std::move(namespaces)) {
  for (const std::string& s : d_) {
    os_ << "namespace " << s << " {\n";
  }
}
NamespaceScope::~NamespaceScope() {
  for (auto i = d_.rbegin(); i != d_.rend(); ++i) {
    os_ << "}  // namespace " << *i << "\n";
  }
}

IncludeGuardScope::IncludeGuardScope(std::ostream& os, std::string file_name)
    : os_(os),
      d_("V8_GEN_TORQUE_GENERATED_" + CapifyStringWithUnderscores(file_name) +
         "_") {
  os_ << "#ifndef " << d_ << "\n";
  os_ << "#define " << d_ << "\n\n";
}
IncludeGuardScope::~IncludeGuardScope() { os_ << "#endif  // " << d_ << "\n"; }

IncludeObjectMacrosScope::IncludeObjectMacrosScope(std::ostream& os) : os_(os) {
  os_ << "\n// Has to be the last include (doesn't have include guards):\n"
         "#include \"src/objects/object-macros.h\"\n";
}
IncludeObjectMacrosScope::~IncludeObjectMacrosScope() {
  os_ << "\n#include \"src/objects/object-macros-undef.h\"\n";
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
