// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

assertFalse(undefined === _WASMEXP_);
assertFalse(undefined == _WASMEXP_);
assertEquals("function", typeof _WASMEXP_.verifyModule);
assertEquals("function", typeof _WASMEXP_.verifyFunction);
assertEquals("function", typeof _WASMEXP_.compileRun);
