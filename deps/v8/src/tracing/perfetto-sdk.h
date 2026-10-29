// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_PERFETTO_SDK_H_
#define V8_TRACING_PERFETTO_SDK_H_

// Include first to ensure that V8_USE_PERFETTO can be defined before use.
#include "include/v8config.h"

#if defined(V8_USE_PERFETTO)

// For now most of v8 uses legacy trace events.
// This must be defined before including any perfetto headers.
#define PERFETTO_ENABLE_LEGACY_TRACE_EVENTS 1

// Use perfetto SDK header perfetto.h if V8_USE_PERFETTO_SDK is defined.
#if defined(V8_USE_PERFETTO_SDK)

#include "perfetto.h"  // NOLINT(build/include_directory)

#else

// These headers must be available in Perfetto SDK as well.
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/tracing/data_source.h"
#include "perfetto/tracing/track_event.h"
#include "perfetto/tracing/track_event_legacy.h"
#include "protos/perfetto/common/builtin_clock.pbzero.h"
#include "protos/perfetto/common/data_source_descriptor.gen.h"
#include "protos/perfetto/config/chrome/v8_config.gen.h"
#include "protos/perfetto/trace/chrome/v8.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"

#endif  // defined(V8_USE_PERFETTO_SDK)

#endif  // defined(V8_USE_PERFETTO)

#endif  // V8_TRACING_PERFETTO_SDK_H_
