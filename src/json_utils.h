#ifndef SRC_JSON_UTILS_H_
#define SRC_JSON_UTILS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <iomanip>
#include <ostream>
#include <limits>
#include <string>

namespace node {

std::string EscapeJsonChars(const std::string& str);
std::string Reindent(const std::string& str, int indentation);

// JSON compiler definitions.
class JSONWriter {
 public:
  JSONWriter(std::ostream& out, bool compact)
    : out_(out), compact_(compact) {}

 private:
  inline void indent() { indent_ += 2; }
  inline void deindent() { indent_ -= 2; }
  inline void advance() {
    if (compact_) return;
    for (int i = 0; i < indent_; i++) out_ << ' ';
  }
  inline void write_one_space() {
    if (compact_) return;
    out_ << ' ';
  }
  inline void write_new_line() {
    if (compact_) return;
    out_ << '\n';
  }

 public:
  inline void json_start() {
    if (state_ == kAfterValue) out_ << ',';
    write_new_line();
    advance();
    out_ << '{';
    indent();
    state_ = kObjectStart;
  }

  inline void json_end() {
    write_new_line();
    deindent();
    advance();
    out_ << '}';
    state_ = kAfterValue;
  }
  template <typename T>
  inline void json_objectstart(T key) {
    if (state_ == kAfterValue) out_ << ',';
    write_new_line();
    advance();
    write_string(key);
    out_ << ':';
    write_one_space();
    out_ << '{';
    indent();
    state_ = kObjectStart;
  }

  template <typename T>
  inline void json_arraystart(T key) {
    if (state_ == kAfterValue) out_ << ',';
    write_new_line();
    advance();
    write_string(key);
    out_ << ':';
    write_one_space();
    out_ << '[';
    indent();
    state_ = kObjectStart;
  }
  inline void json_objectend() {
    write_new_line();
    deindent();
    advance();
    out_ << '}';
    if (indent_ == 0) {
      // Top-level object is complete, so end the line.
      out_ << '\n';
    }
    state_ = kAfterValue;
  }

  inline void json_arrayend() {
    write_new_line();
    deindent();
    advance();
    out_ << ']';
    state_ = kAfterValue;
  }
  template <typename T, typename U>
  inline void json_keyvalue(const T& key, const U& value) {
    if (state_ == kAfterValue) out_ << ',';
    write_new_line();
    advance();
    write_string(key);
    out_ << ':';
    write_one_space();
    write_value(value);
    state_ = kAfterValue;
  }

  template <typename U>
  inline void json_element(const U& value) {
    if (state_ == kAfterValue) out_ << ',';
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
  template <typename T,
            typename test_for_number = typename std::
                enable_if<std::numeric_limits<T>::is_specialized, bool>::type>
  inline void write_value(T number) {
    if (std::is_same<T, bool>::value)
      out_ << (number ? "true" : "false");
    else
      out_ << number;
  }

  inline void write_value(Null null) { out_ << "null"; }
  inline void write_value(const char* str) { write_string(str); }
  inline void write_value(const std::string& str) { write_string(str); }

  inline void write_value(const ForeignJSON& json) {
    out_ << Reindent(json.as_string, indent_);
  }

  inline void write_string(const std::string& str) {
    out_ << '"' << EscapeJsonChars(str) << '"';
  }
  inline void write_string(const char* str) { write_string(std::string(str)); }

  enum JSONState { kObjectStart, kAfterValue };
  std::ostream& out_;
  bool compact_;
  int indent_ = 0;
  int state_ = kObjectStart;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_JSON_UTILS_H_
