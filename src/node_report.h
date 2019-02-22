#ifndef SRC_NODE_REPORT_H_
#define SRC_NODE_REPORT_H_

#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "v8.h"

#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>

#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace report {

#ifdef _WIN32
typedef SYSTEMTIME TIME_TYPE;
#define PATHSEP "\\"
#else  // UNIX, OSX
typedef struct tm TIME_TYPE;
#define PATHSEP "/"
#endif

// Function declarations - functions in src/node_report.cc
std::string TriggerNodeReport(v8::Isolate* isolate,
                              node::Environment* env,
                              const char* message,
                              const char* location,
                              std::string name,
                              v8::Local<v8::String> stackstr);
void GetNodeReport(v8::Isolate* isolate,
                   node::Environment* env,
                   const char* message,
                   const char* location,
                   v8::Local<v8::String> stackstr,
                   std::ostream& out);

// Function declarations - utility functions in src/node_report_utils.cc
void WalkHandle(uv_handle_t* h, void* arg);
std::string EscapeJsonChars(const std::string& str);

template <typename T>
std::string ValueToHexString(T value) {
  std::stringstream hex;

  hex << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex <<
    value;
  return hex.str();
}

// Function declarations - export functions in src/node_report_module.cc
void TriggerReport(const v8::FunctionCallbackInfo<v8::Value>& info);
void GetReport(const v8::FunctionCallbackInfo<v8::Value>& info);

// Node.js boot time - defined in src/node.cc
extern double prog_start_time;

// JSON compiler definitions.
class JSONWriter {
 public:
  explicit JSONWriter(std::ostream& out) : out_(out) {}

  inline void indent() { indent_ += 2; }
  inline void deindent() { indent_ -= 2; }
  inline void advance() {
    for (int i = 0; i < indent_; i++) out_ << ' ';
  }

  inline void json_start() {
    if (state_ == kAfterValue) out_ << ',';
    out_ << '\n';
    advance();
    out_ << '{';
    indent();
    state_ = kObjectStart;
  }

  inline void json_end() {
    out_ << '\n';
    deindent();
    advance();
    out_ << '}';
    state_ = kAfterValue;
  }
  template <typename T>
  inline void json_objectstart(T key) {
    if (state_ == kAfterValue) out_ << ',';
    out_ << '\n';
    advance();
    write_string(key);
    out_ << ": {";
    indent();
    state_ = kObjectStart;
  }

  template <typename T>
  inline void json_arraystart(T key) {
    if (state_ == kAfterValue) out_ << ',';
    out_ << '\n';
    advance();
    write_string(key);
    out_ << ": [";
    indent();
    state_ = kObjectStart;
  }
  inline void json_objectend() {
    out_ << '\n';
    deindent();
    advance();
    out_ << '}';
    state_ = kAfterValue;
  }

  inline void json_arrayend() {
    out_ << '\n';
    deindent();
    advance();
    out_ << ']';
    state_ = kAfterValue;
  }
  template <typename T, typename U>
  inline void json_keyvalue(const T& key, const U& value) {
    if (state_ == kAfterValue) out_ << ',';
    out_ << '\n';
    advance();
    write_string(key);
    out_ << ": ";
    write_value(value);
    state_ = kAfterValue;
  }

  template <typename U>
  inline void json_element(const U& value) {
    if (state_ == kAfterValue) out_ << ',';
    out_ << '\n';
    advance();
    write_value(value);
    state_ = kAfterValue;
  }

  struct Null {};  // Usable as a JSON value.

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

  inline void write_string(const std::string& str) {
    out_ << '"' << EscapeJsonChars(str) << '"';
  }
  inline void write_string(const char* str) { write_string(std::string(str)); }

  enum JSONState { kObjectStart, kAfterValue };
  std::ostream& out_;
  int indent_ = 0;
  int state_ = kObjectStart;
};

}  // namespace report

#endif  // SRC_NODE_REPORT_H_
