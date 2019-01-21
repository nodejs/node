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

// Function declarations - utility functions in src/utilities.cc
void ReportEndpoints(uv_handle_t* h, std::ostringstream& out);
void WalkHandle(uv_handle_t* h, void* arg);
std::string EscapeJsonChars(const std::string& str);

// Function declarations - export functions in src/node_report_module.cc
void TriggerReport(const v8::FunctionCallbackInfo<v8::Value>& info);
void GetReport(const v8::FunctionCallbackInfo<v8::Value>& info);

// Node.js boot time - defined in src/node.cc
extern double prog_start_time;

// JSON compiler definitions.
class JSONWriter {
 public:
  explicit JSONWriter(std::ostream& out)
      : out_(out), indent_(0), state_(JSONOBJECT) {}

  inline void indent() { indent_ += 2; }
  inline void deindent() { indent_ -= 2; }
  inline void advance() {
    for (int i = 0; i < indent_; i++) out_ << " ";
  }

  inline void json_start() {
    if (state_ == JSONVALUE) out_ << ",";
    out_ << "\n";
    advance();
    out_ << "{";
    indent();
    state_ = JSONOBJECT;
  }

  inline void json_end() {
    out_ << "\n";
    deindent();
    advance();
    out_ << "}";
    state_ = JSONVALUE;
  }
  template <typename T>
  inline void json_objectstart(T key) {
    if (state_ == JSONVALUE) out_ << ",";
    out_ << "\n";
    advance();
    out_ << "\"" << key << "\""
         << ": {";
    indent();
    state_ = JSONOBJECT;
  }

  template <typename T>
  inline void json_arraystart(T key) {
    if (state_ == JSONVALUE) out_ << ",";
    out_ << "\n";
    advance();
    out_ << "\"" << key << "\""
         << ": [";
    indent();
    state_ = JSONOBJECT;
  }
  inline void json_objectend() {
    out_ << "\n";
    deindent();
    advance();
    out_ << "}";
    state_ = JSONVALUE;
  }

  inline void json_arrayend() {
    out_ << "\n";
    deindent();
    advance();
    out_ << "]";
    state_ = JSONVALUE;
  }
  template <typename T, typename U>
  inline void json_keyvalue(T key, U value) {
    if (state_ == JSONVALUE) out_ << ",";
    out_ << "\n";
    advance();
    out_ << "\"" << key << "\""
         << ": "
         << "\"";
    out_ << EscapeJsonChars(value) << "\"";
    state_ = JSONVALUE;
  }

  template <typename U>
  inline void json_element(U value) {
    if (state_ == JSONVALUE) out_ << ",";
    out_ << "\n";
    advance();
    out_ << "\"" << EscapeJsonChars(value) << "\"";
    state_ = JSONVALUE;
  }

 private:
  enum JSONState { JSONOBJECT, JSONVALUE };
  std::ostream& out_;
  int indent_;
  int state_;
};

}  // namespace report

#endif  // SRC_NODE_REPORT_H_
