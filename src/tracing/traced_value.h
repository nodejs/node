// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_TRACING_TRACED_VALUE_H_
#define SRC_TRACING_TRACED_VALUE_H_

#include "v8-platform.h"

#include <cstdint>
#include <span>
#include <string>

namespace node {
namespace tracing {

template <typename T>
std::unique_ptr<v8::ConvertableToTraceFormat> CastTracedValue(const T& value) {
  return value.Cast();
}

class EnvironmentArgs {
 public:
  EnvironmentArgs(std::span<const std::string> args,
                  std::span<const std::string> exec_args)
      : args_(args), exec_args_(exec_args) {}

  std::unique_ptr<v8::ConvertableToTraceFormat> Cast() const;

 private:
  std::span<const std::string> args_;
  std::span<const std::string> exec_args_;
};

class AsyncWrapArgs {
 public:
  AsyncWrapArgs(int64_t execution_async_id, int64_t trigger_async_id)
      : execution_async_id_(execution_async_id),
        trigger_async_id_(trigger_async_id) {}

  std::unique_ptr<v8::ConvertableToTraceFormat> Cast() const;

 private:
  int64_t execution_async_id_;
  int64_t trigger_async_id_;
};

class ProcessMeta {
 public:
  std::unique_ptr<v8::ConvertableToTraceFormat> Cast() const;
};

// Do not use this class directly. Define a custom structured class to provide
// a conversion method so that the class can be used with both V8 legacy
// trace API and perfetto API.
//
// These classes provide a JSON-inspired way to write structed data into traces.
//
// To define how a custom class should be written into the trace, users should
// define one of the two following functions:
// - Foo::Cast(TracedValue) const
//   (preferred for code which depends on perfetto directly)
//
// After defining a conversion method, the object can be used as a
// TRACE_EVENT argument:
//
// Foo foo;
// TRACE_EVENT("cat", "Event", "arg", CastTracedValue(foo));
//
// class Foo {
//   std::unique_ptr<v8::ConvertableToTraceFormat> Cast() const {
//     auto traced_value = tracing::TracedValue::Create();
//     dict->SetInteger("key", 42);
//     dict->SetString("foo", "bar");
//     return traced_value;
//   }
// }
class TracedValue : public v8::ConvertableToTraceFormat {
 public:
  ~TracedValue() override = default;

  static std::unique_ptr<TracedValue> Create();
  static std::unique_ptr<TracedValue> CreateArray();

  void EndDictionary();
  void EndArray();

  // These methods assume that |name| is a long lived "quoted" string.
  void SetInteger(const char* name, int value);
  void SetDouble(const char* name, double value);
  void SetBoolean(const char* name, bool value);
  void SetNull(const char* name);
  void SetString(const char* name, const char* value);
  void SetString(const char* name, const std::string& value) {
    SetString(name, value.c_str());
  }
  void BeginDictionary(const char* name);
  void BeginArray(const char* name);

  void AppendInteger(int);
  void AppendDouble(double);
  void AppendBoolean(bool);
  void AppendNull();
  void AppendString(const char*);
  void AppendString(const std::string& value) { AppendString(value.c_str()); }
  void BeginArray();
  void BeginDictionary();

  // ConvertableToTraceFormat implementation.
  void AppendAsTraceFormat(std::string* out) const override;

  TracedValue(const TracedValue&) = delete;
  TracedValue& operator=(const TracedValue&) = delete;

 private:
  explicit TracedValue(bool root_is_array = false);

  void WriteComma();
  void WriteName(const char* name);

  std::string data_;
  bool first_item_;
  bool root_is_array_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_TRACED_VALUE_H_
