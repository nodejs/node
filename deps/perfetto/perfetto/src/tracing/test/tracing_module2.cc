/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/tracing/test/tracing_module.h"

#include "src/tracing/test/tracing_module_categories.h"

#include <stdio.h>

// This file checks that one track event category list can be shared by two
// compilation units.

namespace tracing_module {

void EmitTrackEvents2() {
  TRACE_EVENT_BEGIN("cat1", "DisabledEventFromModule2");
  TRACE_EVENT_END("cat1");
  TRACE_EVENT_BEGIN("cat4", "DisabledEventFromModule2");
  TRACE_EVENT_END("cat4");
  TRACE_EVENT_BEGIN("cat9", "DisabledEventFromModule2");
  TRACE_EVENT_END("cat9");
  TRACE_EVENT_BEGIN("foo", "FooEventFromModule2");
  TRACE_EVENT_END("foo");
}

}  // namespace tracing_module
