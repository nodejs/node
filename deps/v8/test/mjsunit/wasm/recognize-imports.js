// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let sig_w_w = makeSig([kWasmStringRef], [kWasmStringRef]);
let toLowerCase = builder.addImport("m", "toLowerCase", sig_w_w);

builder.addFunction('call_tolower', sig_w_w).exportFunc().addBody([
  kExprLocalGet, 0,
  kExprCallFunction, toLowerCase,
]);

let module = builder.toModule();

let recognizable = Function.prototype.call.bind(String.prototype.toLowerCase);
let recognizable_imports = { m: { toLowerCase: recognizable } };

let instance1 = new WebAssembly.Instance(module, recognizable_imports);
let call_tolower = instance1.exports.call_tolower;
assertEquals("abc", call_tolower("ABC"));
%WasmTierUpFunction(call_tolower);
assertEquals("abc", call_tolower("ABC"));

// Null should be handled correctly (by throwing the same TypeError that
// JavaScript would throw).
assertThrows(
    () => call_tolower(null), TypeError,
    /String.prototype.toLowerCase called on null or undefined/);

// Creating a second instance with identical imports should not cause
// recompilation.
console.log("Second instance.");
let instance2 = new WebAssembly.Instance(module, recognizable_imports);
assertEquals("def", instance2.exports.call_tolower("DEF"));

// Creating a third instance with different imports must not reuse the
// existing optimized code.
console.log("Third instance.");
let other_imports = { m: { toLowerCase: () => "foo" } };
let instance3 = new WebAssembly.Instance(module, other_imports);
assertEquals("foo", instance3.exports.call_tolower("GHI"));
assertEquals("def", instance2.exports.call_tolower("DEF"));
assertEquals("abc", instance1.exports.call_tolower("ABC"));
