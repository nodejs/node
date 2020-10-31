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

#ifndef INCLUDE_PERFETTO_TRACING_H_
#define INCLUDE_PERFETTO_TRACING_H_

// This headers wraps all the headers necessary to use the public Perfetto
// Tracing API. Embedders should preferably use this one header to avoid having
// to figure out the various set of header required for each class.
// The only exception to this should be large projects where build time is a
// concern (e.g. chromium), which migh prefer sticking to strict IWYU.

#include "perfetto/tracing/buffer_exhausted_policy.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/data_source.h"
#include "perfetto/tracing/platform.h"
#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/tracing_backend.h"
#include "perfetto/tracing/track_event.h"
#include "perfetto/tracing/track_event_interned_data_index.h"
#include "perfetto/tracing/track_event_legacy.h"

#endif  // INCLUDE_PERFETTO_TRACING_H_
