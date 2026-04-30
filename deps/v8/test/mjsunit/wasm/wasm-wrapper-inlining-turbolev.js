// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-inline-js-wasm-wrappers
// Flags: --turbofan --no-stress-maglev

// Reuse the existing TurboFan (Sea-of-Nodes) JS-to-Wasm wrapper inlining tests
// to verify the same scenarios work with Turbolev wrapper inlining.
d8.file.execute('test/mjsunit/wasm/wasm-wrapper-inlining.js');
