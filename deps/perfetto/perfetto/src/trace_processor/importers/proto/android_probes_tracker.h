/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_TRACKER_H_

#include <set>

#include "perfetto/ext/base/optional.h"

#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class AndroidProbesTracker : public Destructible {
 public:
  explicit AndroidProbesTracker(TraceProcessorContext*);
  ~AndroidProbesTracker() override;

  static AndroidProbesTracker* GetOrCreate(TraceProcessorContext* context) {
    if (!context->android_probes_tracker) {
      context->android_probes_tracker.reset(new AndroidProbesTracker(context));
    }
    return static_cast<AndroidProbesTracker*>(
        context->android_probes_tracker.get());
  }

  bool ShouldInsertPackage(const std::string& package_name) const {
    auto it = seen_packages_.find(package_name);
    return it == seen_packages_.end();
  }

  void InsertedPackage(std::string package_name) {
    seen_packages_.emplace(std::move(package_name));
  }

 private:
  std::set<std::string> seen_packages_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_TRACKER_H_
