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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACKER_H_

#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/types/destructible.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace Json {
class Value;
}

namespace perfetto {
namespace trace_processor {

class JsonTracker : public Destructible {
 public:
  JsonTracker(const JsonTracker&) = delete;
  JsonTracker& operator=(const JsonTracker&) = delete;
  explicit JsonTracker(TraceProcessorContext*);
  virtual ~JsonTracker();

  static JsonTracker* GetOrCreate(TraceProcessorContext* context) {
    if (!context->json_tracker) {
      context->json_tracker.reset(new JsonTracker(context));
    }
    return static_cast<JsonTracker*>(context->json_tracker.get());
  }

  void SetTimeUnit(json::TimeUnit time_unit) { time_unit_ = time_unit; }

  base::Optional<int64_t> CoerceToTs(const Json::Value& value) {
    return json::CoerceToTs(time_unit_, value);
  }

 private:
  json::TimeUnit time_unit_ = json::TimeUnit::kUs;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACKER_H_
