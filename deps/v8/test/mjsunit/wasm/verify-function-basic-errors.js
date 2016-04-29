// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

function Foo() { }

assertThrows(function() { Wasm.verifyFunction(); })
assertThrows(function() { Wasm.verifyFunction(0); })
assertThrows(function() { Wasm.verifyFunction("s"); })
assertThrows(function() { Wasm.verifyFunction(undefined); })
assertThrows(function() { Wasm.verifyFunction(1.1); })
assertThrows(function() { Wasm.verifyFunction(1/0); })
assertThrows(function() { Wasm.verifyFunction(null); })
assertThrows(function() { Wasm.verifyFunction(new Foo()); })
assertThrows(function() { Wasm.verifyFunction(new ArrayBuffer(0)); })
assertThrows(function() { Wasm.verifyFunction(new ArrayBuffer(140000)); })
