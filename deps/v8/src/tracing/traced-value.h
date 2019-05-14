// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_TRACED_VALUE_H_
#define V8_TRACING_TRACED_VALUE_H_

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "include/v8-platform.h"
#include "src/base/macros.h"

namespace v8 {
namespace tracing {

class TracedValue : public ConvertableToTraceFormat {
 public:
  ~TracedValue() override;

  static std::unique_ptr<TracedValue> Create();

  void EndDictionary();
  void EndArray();

  // These methods assume that |name| is a long lived "quoted" string.
  void SetInteger(const char* name, int value);
  void SetDouble(const char* name, double value);
  void SetBoolean(const char* name, bool value);
  void SetString(const char* name, const char* value);
  void SetString(const char* name, const std::string& value) {
    SetString(name, value.c_str());
  }
  void BeginDictionary(const char* name);
  void BeginArray(const char* name);

  void AppendInteger(int);
  void AppendDouble(double);
  void AppendBoolean(bool);
  void AppendString(const char*);
  void AppendString(const std::string& value) { AppendString(value.c_str()); }
  void BeginArray();
  void BeginDictionary();

  // ConvertableToTraceFormat implementation.
  void AppendAsTraceFormat(std::string* out) const override;

 private:
  TracedValue();

  void WriteComma();
  void WriteName(const char* name);

#ifdef DEBUG
  // In debug builds checks the pairings of {Begin,End}{Dictionary,Array}
  std::vector<bool> nesting_stack_;
#endif

  std::string data_;
  bool first_item_;

  DISALLOW_COPY_AND_ASSIGN(TracedValue);
};

}  // namespace tracing
}  // namespace v8

#endif  // V8_TRACING_TRACED_VALUE_H_
