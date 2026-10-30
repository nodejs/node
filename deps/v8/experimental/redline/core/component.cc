// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/component.h"

#include "experimental/redline/base/log.h"

namespace redline {

std::string FormatStatsTable(std::string_view title,
                             const std::vector<const Component*>& components) {
  std::string out = StringPrintf(
      "%.*s\n  %-26s %9s %9s %7s %9s %6s %6s %6s %6s %6s %7s %9s %8s\n",
      static_cast<int>(title.size()), title.data(), "component", "invoked",
      "produced", "invalid", "executed", "ok%", "fail", "tmo", "crash", "new",
      "edges", "spent", "ms");
  for (const Component* component : components) {
    ComponentStats::Snapshot s = component->stats().snapshot();
    if (s.invocations == 0) {
      continue;
    }
    out += StringPrintf(
        "  %-26s %9llu %9llu %7llu %9llu %5.1f%% %6llu %6llu %6llu %6llu %7llu "
        "%9llu %8llu\n",
        component->name().c_str(),
        static_cast<unsigned long long>(s.invocations),
        static_cast<unsigned long long>(s.produced),
        static_cast<unsigned long long>(s.invalid),
        static_cast<unsigned long long>(s.executed), s.success_rate(),
        static_cast<unsigned long long>(s.failed),
        static_cast<unsigned long long>(s.timed_out),
        static_cast<unsigned long long>(s.crashed),
        static_cast<unsigned long long>(s.interesting),
        static_cast<unsigned long long>(s.new_edges),
        static_cast<unsigned long long>(s.spent),
        static_cast<unsigned long long>(s.time_us / 1000));
  }
  return out;
}

}  // namespace redline
