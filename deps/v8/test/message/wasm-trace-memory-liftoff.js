// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-opt --trace-wasm-memory --liftoff
// Flags: --no-wasm-tier-up --experimental-wasm-simd

// Force enable sse3 and sse4-1, since that will determine which execution tier
// we use, and thus the expected output message will differ.
load("test/message/wasm-trace-memory.js");
