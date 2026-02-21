// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let empty_module = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0]);
let module = new WebAssembly.Module(empty_module);
let instance = new WebAssembly.Instance(module, {});

// We don't care what this returns as long as it doesn't crash.
%BuildRefTypeBitfield(123, instance);
// Issue 399769594 pointed out that we need even more robustness:
%BuildRefTypeBitfield(-1, instance);
