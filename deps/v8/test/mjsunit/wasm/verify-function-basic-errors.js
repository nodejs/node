// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

function Foo() { }

assertThrows(function() { _WASMEXP_.verifyFunction(); })
assertThrows(function() { _WASMEXP_.verifyFunction(0); })
assertThrows(function() { _WASMEXP_.verifyFunction("s"); })
assertThrows(function() { _WASMEXP_.verifyFunction(undefined); })
assertThrows(function() { _WASMEXP_.verifyFunction(1.1); })
assertThrows(function() { _WASMEXP_.verifyFunction(1/0); })
assertThrows(function() { _WASMEXP_.verifyFunction(null); })
assertThrows(function() { _WASMEXP_.verifyFunction(new Foo()); })
assertThrows(function() { _WASMEXP_.verifyFunction(new ArrayBuffer(0)); })
assertThrows(function() { _WASMEXP_.verifyFunction(new ArrayBuffer(140000)); })
