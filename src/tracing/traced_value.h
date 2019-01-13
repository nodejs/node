// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_TRACING_TRACED_VALUE_H_
#define SRC_TRACING_TRACED_VALUE_H_

#include "node.h"
#include "util.h"
#include "v8.h"

#include <stddef.h>
#include <memory>
#include <string>

namespace node {
namespace tracing {

class TracedValue : public v8::ConvertableToTraceFormat {
 public:
  ~TracedValue() override;

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

 private:
  explicit TracedValue(bool root_is_array = false);

  void WriteComma();
  void WriteName(const char* name);

  std::string data_;
  bool first_item_;
  bool root_is_array_;

  DISALLOW_COPY_AND_ASSIGN(TracedValue);
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_TRACED_VALUE_H_
