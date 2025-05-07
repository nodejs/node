// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Force eager compilation; if Liftoff bails out (because of missing SSSE3) it
// compiles with TurboFan, which eventually triggers caching. This was missing
// support for --single-threaded.
// Flags: --no-enable-ssse3 --no-wasm-lazy-compilation --single-threaded

for (let i = 0; i < 100; i++) {
  %WasmGenerateRandomModule();
}
