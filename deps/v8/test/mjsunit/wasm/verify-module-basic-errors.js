// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

function Foo() { }

assertThrows(function() { Wasm.verifyModule(); })
assertThrows(function() { Wasm.verifyModule(0); })
assertThrows(function() { Wasm.verifyModule("s"); })
assertThrows(function() { Wasm.verifyModule(undefined); })
assertThrows(function() { Wasm.verifyModule(1.1); })
assertThrows(function() { Wasm.verifyModule(1/0); })
assertThrows(function() { Wasm.verifyModule(null); })
assertThrows(function() { Wasm.verifyModule(new Foo()); })
assertThrows(function() { Wasm.verifyModule(new ArrayBuffer(0)); })
assertThrows(function() { Wasm.verifyModule(new ArrayBuffer(7)); })
