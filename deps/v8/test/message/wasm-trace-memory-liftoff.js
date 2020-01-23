// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-opt --trace-wasm-memory --liftoff --no-future
// Flags: --no-wasm-tier-up --experimental-wasm-simd

// liftoff does not support simd128, so the s128 load and store traces are in
// the turbofan tier and not liftoff
load("test/message/wasm-trace-memory.js");
