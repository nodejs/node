// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_ETW_ETW_PROVIDER_WIN_H_
#define V8_LIBPLATFORM_ETW_ETW_PROVIDER_WIN_H_

// This file defines all the ETW Provider functions.
#include <windows.h>
#ifndef VOID
#define VOID void
#endif
#include <TraceLoggingProvider.h>
#include <evntprov.h>
#include <evntrace.h>  // defines TRACE_LEVEL_* and EVENT_TRACE_TYPE_*

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
#endif

#ifndef V8_ETW_GUID
#define V8_ETW_GUID \
  0x57277741, 0x3638, 0x4A4B, 0xBD, 0xBA, 0x0A, 0xC6, 0xE4, 0x5D, 0xA5, 0x6C
#endif  // V8_ETW_GUID

#define V8_DECLARE_TRACELOGGING_PROVIDER(v8Provider) \
  TRACELOGGING_DECLARE_PROVIDER(v8Provider);

#define V8_DEFINE_TRACELOGGING_PROVIDER(v8Provider) \
  TRACELOGGING_DEFINE_PROVIDER(v8Provider, "V8.js", (V8_ETW_GUID));

#endif  // V8_LIBPLATFORM_ETW_ETW_PROVIDER_WIN_H_
