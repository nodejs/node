#pragma once

#include "node.h"
#include "node_buffer.h"
#include "uv.h"
#include "util.h"

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include <iomanip>

namespace report {

// Function declarations - functions in src/node_report.cc
std::string TriggerNodeReport(v8::Isolate* isolate,
                              node::Environment* env,
                              const char* message,
                              const char* trigger,
                              const std::string& name,
                              v8::Local<v8::Value> error);
void GetNodeReport(v8::Isolate* isolate,
                   node::Environment* env,
                   const char* message,
                   const char* trigger,
                   v8::Local<v8::Value> error,
                   std::ostream& out);

// Function declarations - utility functions in src/node_report_utils.cc
void WalkHandle(uv_handle_t* h, void* arg);

template <typename T>
std::string ValueToHexString(T value) {
  std::stringstream hex;

  hex << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex <<
    value;
  return hex.str();
}

// Function declarations - export functions in src/node_report_module.cc
void WriteReport(const v8::FunctionCallbackInfo<v8::Value>& info);
void GetReport(const v8::FunctionCallbackInfo<v8::Value>& info);

}  // namespace report
