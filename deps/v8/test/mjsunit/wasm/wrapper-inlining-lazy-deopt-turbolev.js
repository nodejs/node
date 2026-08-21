// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turbofan --no-stress-maglev

// Reuse the existing lazy deopt tests from the TurboFan (Sea-of-Nodes) inlining
// pipeline to verify the same scenarios work with Turbolev wrapper inlining.
d8.file.execute('test/mjsunit/wasm/wrapper-inlining-lazy-deopt.js');
