// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let charCodeAt = builder.addImport(
    'wasm:js-string', 'charCodeAt',
    makeSig([kWasmExternRef, kWasmI32], [kWasmI32]));

builder.addFunction('test', kSig_i_r).exportFunc().addBody([
  kExprLocalGet, 0, kExprI32Const, 0, kExprCallFunction, charCodeAt
]);

let buffer = builder.toBuffer();

function assertBuiltinAvailable(options) {
  let module = new WebAssembly.Module(buffer, options);
  let instance = new WebAssembly.Instance(module);
  assertEquals(97, instance.exports.test('abc'));
}
function assertBuiltinNotUsed(options) {
  let module = new WebAssembly.Module(buffer, options);
  let instance = new WebAssembly.Instance(module, {
    'wasm:js-string': { charCodeAt: (str, index) => 999 }
  });
  assertEquals(999, instance.exports.test('abc'));
}
function assertCompilationThrows(options) {
  assertThrows(() => new WebAssembly.Module(buffer, options));
}

let kBuiltins = { builtins: ["js-string"] };

// We can validate modules, with or without compile-time imports.
(() => {
  assertTrue(WebAssembly.validate(buffer, kBuiltins));
  assertTrue(WebAssembly.validate(buffer));

  // We can even validate modules that don't have any imports at all.
  let no_import_builder = new WasmModuleBuilder();
  no_import_builder.addFunction('test', kSig_i_i).exportFunc().addBody([
    kExprLocalGet, 0,
  ]);
  assertTrue(WebAssembly.validate(no_import_builder.toBuffer(), kBuiltins));
})();

// Check that the test framework works.
assertBuiltinAvailable(kBuiltins);
assertBuiltinNotUsed(undefined);

// Compile-time provided imports are ignored after compilation.
(() => {
  let module = new WebAssembly.Module(buffer, kBuiltins);
  let instance = new WebAssembly.Instance(module, {
    get "js-string"() { throw "unreachable"; }
  });
  let instance2 = new WebAssembly.Instance(module, {
    "js-string": {
      get equals() { throw "unreachable"; }
    }
  });
})();

// Imports that are missing at compile time can be provided later.
(() => {
  let build = new WasmModuleBuilder();
  build.addImport("js-string", "nonexistent", kSig_i_i);
  let module = new WebAssembly.Module(build.toBuffer());
  let import_has_been_retrieved = false;
  let instance = new WebAssembly.Instance(module, {
    "js-string": {
      get nonexistent() { import_has_been_retrieved = true; return (x) => 1; },
    }
  });
  assertTrue(import_has_been_retrieved);
})();

// What if some of the involved values are primitives?
assertBuiltinNotUsed(123);
assertBuiltinNotUsed({builtins: 123});
assertBuiltinNotUsed({builtins: [123]});

// Can the builtins list be an array-like object?
assertBuiltinAvailable({builtins: {length: 1, 0: "js-string"}});
assertBuiltinNotUsed({builtins: {length: 1, 0: 0, 1: "js-string"}});

// Can the builtins list have nonexistent entries?
assertBuiltinAvailable({builtins: [, , "js-string", , ,]});

// Can the builtins list have accessor properties? What if they throw?
assertBuiltinAvailable({builtins: {
  length: 1,
  get 0() { return "js-string"; }
}});
assertCompilationThrows({builtins: {
  length: 1,
  get 0() { throw "js-string"; }
}});
assertCompilationThrows({builtins: {
  get length() { throw "nope"; }
}});
assertCompilationThrows({get builtins() { throw "nope"; }});

// Do we check the builtins list's prototype?
assertBuiltinAvailable({builtins: {__proto__: ["js-string"]}});

// Do we perform `toString` on elements of the builtins list?
assertBuiltinNotUsed({builtins: [{toString: () => "js-string"}]});

// Can the builtins list be a Proxy?
assertBuiltinAvailable({builtins: new Proxy({}, {
  get(target, name) {
    if (name === "length") return 1;
    if (name === "0") return "js-string";
  },
  has(target, name) {
    return name === "length" || name === "0";
  }
})});

// Can the builtins list modify itself?
// The length is read only once, so later updates to it are ignored.
assertBuiltinAvailable({builtins: {
  length: 2,
  get 0() {this.length = 1; return ""},
  1: "js-string"
}});
assertBuiltinNotUsed({builtins: {
  length: 1,
  get 0() {this.length = 2; this[1] = "js-strings"; return ""},
}});
// Values can disappear, of course.
assertBuiltinNotUsed({builtins: {
  length: 2,
  get 0() {delete this[1]; return ""},
  1: "js-string"
}});
