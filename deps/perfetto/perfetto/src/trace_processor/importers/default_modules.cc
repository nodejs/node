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

#include "src/trace_processor/importers/default_modules.h"
#include "src/trace_processor/importers/ftrace/ftrace_module.h"
#include "src/trace_processor/importers/proto/profile_module.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/importers/proto/track_event_module.h"

namespace perfetto {
namespace trace_processor {

void RegisterDefaultModules(TraceProcessorContext* context) {
  context->modules.emplace_back(new FtraceModule());
  // Ftrace module is special, because it has one extra method for parsing
  // ftrace packets. So we need to store a pointer to it separately.
  context->ftrace_module =
      static_cast<FtraceModule*>(context->modules.back().get());

  context->modules.emplace_back(new TrackEventModule(context));
  context->modules.emplace_back(new ProfileModule(context));
}

}  // namespace trace_processor
}  // namespace perfetto
