// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform-posix.h"

namespace v8 {
namespace base {

class PosixDefaultTimezoneCache : public PosixTimezoneCache {
 public:
  const char* LocalTimezone(double time_ms) override;
  double LocalTimeOffset() override;

  ~PosixDefaultTimezoneCache() override {}
};

}  // namespace base
}  // namespace v8
