// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "include/libplatform/v8-tracing.h"
#include "src/base/logging.h"

namespace v8 {

class Isolate;

namespace platform {
namespace tracing {

TraceConfig* TraceConfig::CreateDefaultTraceConfig() {
  TraceConfig* trace_config = new TraceConfig();
  trace_config->included_categories_.push_back("v8");
  return trace_config;
}

bool TraceConfig::IsCategoryGroupEnabled(const char* category_group) const {
  std::stringstream category_stream(category_group);
  while (category_stream.good()) {
    std::string category;
    getline(category_stream, category, ',');
    for (const auto& included_category : included_categories_) {
      if (category == included_category) return true;
    }
  }
  return false;
}

void TraceConfig::AddIncludedCategory(const char* included_category) {
  DCHECK(included_category != NULL && strlen(included_category) > 0);
  included_categories_.push_back(included_category);
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
