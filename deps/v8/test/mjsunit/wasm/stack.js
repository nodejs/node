// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var expected = "Error\n" +
    // The line numbers below will change as this test gains / loses lines..
    "    at STACK (stack.js:24:11)\n" +     // --
    "    at <WASM> (<anonymous>)\n" +       // TODO(jfb): wasm stack here.
    "    at testStack (stack.js:38:18)\n" + // --
    "    at stack.js:40:3";                 // --

// The stack trace contains file path, only keep "stack.js".
function stripPath(s) {
  return s.replace(/[^ (]*stack\.js/g, "stack.js");
}

var stack;
function STACK() {
  var e = new Error();
  stack = e.stack;
}

(function testStack() {
  var builder = new WasmModuleBuilder();

  builder.addImport("func", [kAstStmt]);

  builder.addFunction(undefined, [kAstStmt])
    .addBody([kExprCallImport, 0])
    .exportAs("main");

  var module = builder.instantiate({func: STACK});
  module.exports.main();
  assertEquals(expected, stripPath(stack));
})();
