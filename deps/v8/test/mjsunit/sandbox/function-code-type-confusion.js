// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-memory-corruption-api

let builtin_names = Sandbox.getBuiltinNames();
let id = builtin_names.indexOf("JSToWasmWrapper");

function F0() { }
// Trigger Type Confusion by forcing the JS Function to use the Wasm builtin.
Sandbox.setFunctionCodeToBuiltin(F0, id);
F0();
