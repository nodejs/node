// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-tracing

var binding = d8.getExtrasBindingObject();
binding.trace(
  66,           // TRACE_EVENT_PHASE_BEGIN = 'B' = 66
  "v8",         // category (must be enabled)
  "pocEvent",   // name (must be non-empty)
  null,         // id
  function () { } // data_arg
);

binding.trace(
  66,           // TRACE_EVENT_PHASE_BEGIN = 'B' = 66
  "v8",         // category (must be enabled)
  "pocEvent",   // name (must be non-empty)
  null,         // id
  { a: 1, b: 2 } // data_arg
);
