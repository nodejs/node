// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_TRACING_CATEGORY_OBSERVER_H_
#define V8_TRACING_TRACING_CATEGORY_OBSERVER_H_

#include "include/v8-platform.h"

namespace v8 {
namespace tracing {

class TracingCategoryObserver : public Platform::TraceStateObserver {
 public:
  enum Mode {
    ENABLED_BY_NATIVE = 1 << 0,
    ENABLED_BY_TRACING = 1 << 1,
    ENABLED_BY_SAMPLING = 1 << 2,
  };

  static void SetUp();
  static void TearDown();

  // v8::Platform::TraceStateObserver
  void OnTraceEnabled() final;
  void OnTraceDisabled() final;

 private:
  static TracingCategoryObserver* instance_;
};

}  // namespace tracing
}  // namespace v8

#endif  // V8_TRACING_TRACING_CATEGORY_OBSERVER_H_
