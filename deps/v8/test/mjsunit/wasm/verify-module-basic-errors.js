// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

function Foo() { }

assertThrows(function() { _WASMEXP_.verifyModule(); })
assertThrows(function() { _WASMEXP_.verifyModule(0); })
assertThrows(function() { _WASMEXP_.verifyModule("s"); })
assertThrows(function() { _WASMEXP_.verifyModule(undefined); })
assertThrows(function() { _WASMEXP_.verifyModule(1.1); })
assertThrows(function() { _WASMEXP_.verifyModule(1/0); })
assertThrows(function() { _WASMEXP_.verifyModule(null); })
assertThrows(function() { _WASMEXP_.verifyModule(new Foo()); })
assertThrows(function() { _WASMEXP_.verifyModule(new ArrayBuffer(0)); })
assertThrows(function() { _WASMEXP_.verifyModule(new ArrayBuffer(7)); })
