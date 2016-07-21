// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

assertFalse(undefined === Wasm);
assertFalse(undefined == Wasm);
assertEquals("function", typeof Wasm.verifyModule);
assertEquals("function", typeof Wasm.verifyFunction);
assertEquals("function", typeof Wasm.instantiateModule);
assertEquals("function", typeof Wasm.instantiateModuleFromAsm);
assertFalse(undefined == Wasm.experimentalVersion);
