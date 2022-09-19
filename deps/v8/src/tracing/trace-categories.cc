// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/trace-categories.h"

#if defined(V8_USE_PERFETTO)
PERFETTO_TRACK_EVENT_STATIC_STORAGE();
#endif
