#ifndef SRC_JSON_UTILS_H_
#define SRC_JSON_UTILS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_concepts.h"

#include <charconv>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>

namespace node {

constexpr bool NeedsJsonEscape(std::string_view str) {
  for (const char c : str) {
    if (c == '\\' || c == '"' || c < 0x20) return true;
  }
  return false;
}

std::string EscapeJsonChars(std::string_view str);
std::string Reindent(const std::string& str, int indentation);

// JSON compiler definitions.
class JSONWriter {
 public:
  JSONWriter(std::ostream& out, bool compact) : out_(&out), compact_(compact) {}
  JSONWriter(std::string& out, bool compact)
      : string_out_(&out), compact_(compact) {}

 private:
  inline void write(char ch) {
    if (string_out_ != nullptr)
      *string_out_ += ch;
    else
      *out_ << ch;
  }
  inline void write(std::string_view str) {
    if (string_out_ != nullptr)
      string_out_->append(str);
    else
      *out_ << str;
  }
  inline void indent() { indent_ += 2; }
  inline void deindent() { indent_ -= 2; }
  inline void advance() {
    if (compact_) return;
    for (int i = 0; i < indent_; i++) write(' ');
  }
  inline void write_one_space() {
    if (compact_) return;
    write(' ');
  }
  inline void write_new_line() {
    if (compact_) return;
    write('\n');
  }

 public:
  inline void json_start() {
    if (state_ == kAfterValue) write(',');
    write_new_line();
    advance();
    write('{');
    indent();
    state_ = kObjectStart;
  }

  inline void json_end() {
    write_new_line();
    deindent();
    advance();
    write('}');
    state_ = kAfterValue;
  }
  template <typename T>
  inline void json_objectstart(T key) {
    if (state_ == kAfterValue) write(',');
    write_new_line();
    advance();
    write_string(key);
    write(':');
    write_one_space();
    write('{');
    indent();
    state_ = kObjectStart;
  }

  template <typename T>
  inline void json_arraystart(T key) {
    if (state_ == kAfterValue) write(',');
    write_new_line();
    advance();
    write_string(key);
    write(':');
    write_one_space();
    write('[');
    indent();
    state_ = kObjectStart;
  }
  inline void json_objectend() {
    write_new_line();
    deindent();
    advance();
    write('}');
    if (indent_ == 0) {
      // Top-level object is complete, so end the line.
      write('\n');
    }
    state_ = kAfterValue;
  }

  inline void json_arrayend() {
    write_new_line();
    deindent();
    advance();
    write(']');
    state_ = kAfterValue;
  }
  template <typename T, typename U>
  inline void json_keyvalue(const T& key, const U& value) {
    if (state_ == kAfterValue) write(',');
    write_new_line();
    advance();
    write_string(key);
    write(':');
    write_one_space();
    write_value(value);
    state_ = kAfterValue;
  }

  template <typename U>
  inline void json_element(const U& value) {
    if (state_ == kAfterValue) write(',');
    write_new_line();
    advance();
    write_value(value);
    state_ = kAfterValue;
  }

  struct Null {};  // Usable as a JSON value.

  struct ForeignJSON {
    std::string as_string;
  };

 private:
  template <NumericValue T>
  inline void write_value(T number) {
    if constexpr (std::is_same_v<T, bool>) {
      write(number ? "true" : "false");
    } else if constexpr (std::is_same_v<T, char> ||
                         std::is_same_v<T, signed char> ||
                         std::is_same_v<T, unsigned char>) {
      write(static_cast<char>(number));
    } else {
      if (out_ != nullptr) {
        *out_ << number;
        return;
      }
      // Enough for any integer, or a floating-point value with six digits
      // and an exponent. Match the default ostream precision.
      char buffer[128];
      std::to_chars_result result;
      if constexpr (std::is_floating_point_v<T>) {
        result = std::to_chars(buffer,
                               buffer + sizeof(buffer),
                               number,
                               std::chars_format::general,
                               6);
      } else {
        result = std::to_chars(buffer, buffer + sizeof(buffer), number);
      }
      string_out_->append(buffer, result.ptr);
    }
  }

  inline void write_value(Null null) { write("null"); }
  inline void write_value(std::string_view str) { write_string(str); }

  inline void write_value(const ForeignJSON& json) {
    write(Reindent(json.as_string, indent_));
  }

  inline void write_string(std::string_view str) {
    write('"');
    if (NeedsJsonEscape(str))  // only create temporary std::string if necessary
      write(EscapeJsonChars(str));
    else
      write(str);
    write('"');
  }

  enum JSONState { kObjectStart, kAfterValue };
  std::ostream* out_ = nullptr;
  std::string* string_out_ = nullptr;
  bool compact_;
  int indent_ = 0;
  int state_ = kObjectStart;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_JSON_UTILS_H_
