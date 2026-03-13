// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_TEMPORAL_ZONEINFO64_H_
#define V8_OBJECTS_JS_TEMPORAL_ZONEINFO64_H_

#include "temporal_rs/Provider.d.hpp"

#ifdef V8_INTL_SUPPORT
#include "unicode/udata.h"
#else
#include <vector>
#endif

namespace v8::internal {

// This class wraps timezone data, either loaded from ICU4C or static data,
// to be used with timezone-using temporal_rs APIs
//
// Users should typically access this via
// ZoneInfo64Provider::Singleton()
class ZoneInfo64Provider {
 public:
  static ZoneInfo64Provider& Singleton();
  temporal_rs::Provider& Provider() { return *provider; }

  // Don't make copies of this
  // (Also it may be self-referential)
  ZoneInfo64Provider(const ZoneInfo64Provider&) = delete;
  ZoneInfo64Provider operator=(const ZoneInfo64Provider&) = delete;
  ZoneInfo64Provider(ZoneInfo64Provider&&) = delete;
  ZoneInfo64Provider operator=(ZoneInfo64Provider&&) = delete;

 private:
  // People should be using the singleton
  ZoneInfo64Provider();
  ~ZoneInfo64Provider();
  std::unique_ptr<temporal_rs::Provider> provider;
#ifdef V8_INTL_SUPPORT
  UDataMemory* memory;
#endif
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_JS_TEMPORAL_ZONEINFO64_H_
