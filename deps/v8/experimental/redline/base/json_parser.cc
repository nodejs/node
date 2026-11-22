// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/json_parser.h"

#include <cmath>
#include <cstdlib>

#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"

namespace redline {

namespace {
const JsonValue kNullValue;
}  // namespace

const JsonValue& JsonValue::Get(std::string_view key) const {
  for (const auto& [k, v] : object_) {
    if (k == key) {
      return v;
    }
  }
  return kNullValue;
}

bool JsonValue::Has(std::string_view key) const {
  for (const auto& [k, v] : object_) {
    if (k == key) {
      return true;
    }
  }
  return false;
}

double JsonValue::GetNumber(std::string_view key, double fallback) const {
  const JsonValue& v = Get(key);
  return v.is_number() ? v.as_number() : fallback;
}

int64_t JsonValue::GetInt(std::string_view key, int64_t fallback) const {
  const JsonValue& v = Get(key);
  return v.is_number() ? v.as_int() : fallback;
}

bool JsonValue::GetBool(std::string_view key, bool fallback) const {
  const JsonValue& v = Get(key);
  return v.is_bool() ? v.as_bool() : fallback;
}

std::string JsonValue::GetString(std::string_view key,
                                 std::string_view fallback) const {
  const JsonValue& v = Get(key);
  return v.is_string() ? v.as_string() : std::string(fallback);
}

class JsonParser {
 public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  bool Parse(JsonValue* out, std::string* error) {
    SkipWhitespace();
    if (!ParseValue(out, 0)) {
      *error = StringPrintf("%d:%d: %s", line_, Column(), error_.c_str());
      return false;
    }
    SkipWhitespace();
    if (pos_ != text_.size()) {
      *error = StringPrintf("%d:%d: trailing characters", line_, Column());
      return false;
    }
    return true;
  }

 private:
  static constexpr int kMaxDepth = 64;

  int Column() const {
    size_t line_start = text_.rfind('\n', pos_ == 0 ? 0 : pos_ - 1);
    size_t start = line_start == std::string_view::npos ? 0 : line_start + 1;
    return static_cast<int>(pos_ - start) + 1;
  }

  void SkipWhitespace() {
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if (c == '\n') {
        line_++;
      }
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        break;
      }
      pos_++;
    }
  }

  bool Fail(const char* message) {
    error_ = message;
    return false;
  }

  bool ParseValue(JsonValue* out, int depth) {
    if (depth > kMaxDepth) {
      return Fail("nesting too deep");
    }
    if (pos_ >= text_.size()) {
      return Fail("unexpected end of input");
    }
    char c = text_[pos_];
    if (c == '{') {
      return ParseObject(out, depth);
    }
    if (c == '[') {
      return ParseArray(out, depth);
    }
    if (c == '"') {
      out->type_ = JsonValue::Type::kString;
      return ParseString(&out->string_);
    }
    if (c == 't' || c == 'f' || c == 'n') {
      return ParseKeyword(out);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return ParseNumber(out);
    }
    return Fail("unexpected character");
  }

  bool ParseKeyword(JsonValue* out) {
    std::string_view rest = text_.substr(pos_);
    if (rest.starts_with("true")) {
      pos_ += 4;
      out->type_ = JsonValue::Type::kBool;
      out->bool_ = true;
      return true;
    }
    if (rest.starts_with("false")) {
      pos_ += 5;
      out->type_ = JsonValue::Type::kBool;
      out->bool_ = false;
      return true;
    }
    if (rest.starts_with("null")) {
      pos_ += 4;
      out->type_ = JsonValue::Type::kNull;
      return true;
    }
    return Fail("invalid literal");
  }

  bool ParseNumber(JsonValue* out) {
    size_t start = pos_;
    if (text_[pos_] == '-') {
      pos_++;
    }
    auto digits = [&] {
      size_t before = pos_;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
        pos_++;
      }
      return pos_ > before;
    };
    if (!digits()) {
      return Fail("invalid number");
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      pos_++;
      if (!digits()) {
        return Fail("invalid number");
      }
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      pos_++;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
        pos_++;
      }
      if (!digits()) {
        return Fail("invalid number");
      }
    }
    std::string literal(text_.substr(start, pos_ - start));
    out->type_ = JsonValue::Type::kNumber;
    out->number_ = strtod(literal.c_str(), nullptr);
    return true;
  }

  static void AppendUtf8(std::string* out, uint32_t code_point) {
    if (code_point < 0x80) {
      out->push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800) {
      out->push_back(static_cast<char>(0xC0 | (code_point >> 6)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point < 0x10000) {
      out->push_back(static_cast<char>(0xE0 | (code_point >> 12)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xF0 | (code_point >> 18)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
  }

  bool ParseHex4(uint32_t* out) {
    if (pos_ + 4 > text_.size()) {
      return Fail("truncated \\u escape");
    }
    uint32_t value = 0;
    for (int i = 0; i < 4; i++) {
      char c = text_[pos_++];
      value <<= 4;
      if (c >= '0' && c <= '9') {
        value |= static_cast<uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        value |= static_cast<uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        value |= static_cast<uint32_t>(c - 'A' + 10);
      } else {
        return Fail("invalid \\u escape");
      }
    }
    *out = value;
    return true;
  }

  bool ParseString(std::string* out) {
    pos_++;  // opening quote
    while (true) {
      if (pos_ >= text_.size()) {
        return Fail("unterminated string");
      }
      char c = text_[pos_++];
      if (c == '"') {
        return true;
      }
      if (c == '\n') {
        return Fail("newline in string");
      }
      if (c != '\\') {
        out->push_back(c);
        continue;
      }
      if (pos_ >= text_.size()) {
        return Fail("unterminated escape");
      }
      char e = text_[pos_++];
      switch (e) {
        case '"':
          out->push_back('"');
          break;
        case '\\':
          out->push_back('\\');
          break;
        case '/':
          out->push_back('/');
          break;
        case 'b':
          out->push_back('\b');
          break;
        case 'f':
          out->push_back('\f');
          break;
        case 'n':
          out->push_back('\n');
          break;
        case 'r':
          out->push_back('\r');
          break;
        case 't':
          out->push_back('\t');
          break;
        case 'u': {
          uint32_t code_point;
          if (!ParseHex4(&code_point)) {
            return false;
          }
          if (code_point >= 0xD800 && code_point < 0xDC00 &&
              pos_ + 6 <= text_.size() && text_[pos_] == '\\' &&
              text_[pos_ + 1] == 'u') {
            size_t saved_pos = pos_;
            pos_ += 2;
            uint32_t low;
            if (!ParseHex4(&low)) {
              return false;
            }
            if (low >= 0xDC00 && low <= 0xDFFF) {
              code_point =
                  0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
            } else {
              pos_ = saved_pos;
            }
          }
          AppendUtf8(out, code_point);
          break;
        }
        default:
          return Fail("invalid escape");
      }
    }
  }

  bool ParseArray(JsonValue* out, int depth) {
    pos_++;  // [
    out->type_ = JsonValue::Type::kArray;
    SkipWhitespace();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      pos_++;
      return true;
    }
    while (true) {
      SkipWhitespace();
      out->array_.emplace_back();
      if (!ParseValue(&out->array_.back(), depth + 1)) {
        return false;
      }
      SkipWhitespace();
      if (pos_ >= text_.size()) {
        return Fail("unterminated array");
      }
      char c = text_[pos_++];
      if (c == ']') {
        return true;
      }
      if (c != ',') {
        return Fail("expected ',' or ']'");
      }
    }
  }

  bool ParseObject(JsonValue* out, int depth) {
    pos_++;  // {
    out->type_ = JsonValue::Type::kObject;
    SkipWhitespace();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      pos_++;
      return true;
    }
    while (true) {
      SkipWhitespace();
      if (pos_ >= text_.size() || text_[pos_] != '"') {
        return Fail("expected object key");
      }
      std::string key;
      if (!ParseString(&key)) {
        return false;
      }
      SkipWhitespace();
      if (pos_ >= text_.size() || text_[pos_] != ':') {
        return Fail("expected ':'");
      }
      pos_++;
      SkipWhitespace();
      out->object_.emplace_back(std::move(key), JsonValue());
      if (!ParseValue(&out->object_.back().second, depth + 1)) {
        return false;
      }
      SkipWhitespace();
      if (pos_ >= text_.size()) {
        return Fail("unterminated object");
      }
      char c = text_[pos_++];
      if (c == '}') {
        return true;
      }
      if (c != ',') {
        return Fail("expected ',' or '}'");
      }
    }
  }

  std::string_view text_;
  size_t pos_ = 0;
  int line_ = 1;
  std::string error_;
};

// static
bool JsonValue::Parse(std::string_view text, JsonValue* out,
                      std::string* error) {
  *out = JsonValue();
  JsonParser parser(text);
  return parser.Parse(out, error);
}

std::string JsonValue::ToString() const {
  switch (type_) {
    case Type::kNull:
      return "null";
    case Type::kBool:
      return bool_ ? "true" : "false";
    case Type::kNumber: {
      if (std::isfinite(number_) && number_ == std::floor(number_) &&
          std::fabs(number_) < 1e15) {
        return StringPrintf("%lld", static_cast<long long>(number_));
      }
      return StringPrintf("%.17g", number_);
    }
    case Type::kString:
      return JsonQuote(string_);
    case Type::kArray: {
      std::string out = "[";
      for (size_t i = 0; i < array_.size(); i++) {
        if (i != 0) {
          out += ',';
        }
        out += array_[i].ToString();
      }
      return out + "]";
    }
    case Type::kObject: {
      std::string out = "{";
      for (size_t i = 0; i < object_.size(); i++) {
        if (i != 0) {
          out += ',';
        }
        out += JsonQuote(object_[i].first);
        out += ':';
        out += object_[i].second.ToString();
      }
      return out + "}";
    }
  }
  return "null";
}

}  // namespace redline
