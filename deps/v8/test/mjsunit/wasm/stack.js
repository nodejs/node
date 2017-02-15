// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// The stack trace contains file path, only keep "stack.js".
function stripPath(s) {
  return s.replace(/[^ (]*stack\.js/g, "stack.js");
}

function verifyStack(frames, expected) {
  assertEquals(expected.length, frames.length, "number of frames mismatch");
  expected.forEach(function(exp, i) {
    assertEquals(exp[1], frames[i].getFunctionName(),
        "["+i+"].getFunctionName()");
    assertEquals(exp[2], frames[i].getLineNumber(), "["+i+"].getLineNumber()");
    if (exp[0])
      assertEquals(exp[3], frames[i].getPosition(),
          "["+i+"].getPosition()");
    assertContains(exp[4], frames[i].getFileName(), "["+i+"].getFileName()");
    var toString;
    if (exp[0]) {
      toString = exp[1] + " (<WASM>[" + exp[2] + "]+" + exp[3] + ")";
    } else {
      toString = exp[4] + ":" + exp[2] + ":";
    }
    assertContains(toString, frames[i].toString(), "["+i+"].toString()");
  });
}


var stack;
function STACK() {
  var e = new Error();
  stack = e.stack;
}

var builder = new WasmModuleBuilder();

builder.addImport("func", kSig_v_v);

builder.addFunction("main", kSig_v_v)
  .addBody([kExprCallFunction, 0])
  .exportAs("main");

builder.addFunction("exec_unreachable", kSig_v_v)
  .addBody([kExprUnreachable])
  .exportAs("exec_unreachable");

// Make this function unnamed, just to test also this case.
var mem_oob_func = builder.addFunction(undefined, kSig_i_v)
  // Access the memory at offset -1, to provoke a trap.
  .addBody([kExprI32Const, 0x7f, kExprI32LoadMem8S, 0, 0])
  .exportAs("mem_out_of_bounds");

// Call the mem_out_of_bounds function, in order to have two WASM stack frames.
builder.addFunction("call_mem_out_of_bounds", kSig_i_v)
  .addBody([kExprCallFunction, mem_oob_func.index])
  .exportAs("call_mem_out_of_bounds");

var module = builder.instantiate({func: STACK});

(function testSimpleStack() {
  var expected_string = "Error\n" +
    // The line numbers below will change as this test gains / loses lines..
    "    at STACK (stack.js:39:11)\n" +           // --
    "    at main (<WASM>[1]+1)\n" +               // --
    "    at testSimpleStack (stack.js:76:18)\n" + // --
    "    at stack.js:78:3";                       // --

  module.exports.main();
  assertEquals(expected_string, stripPath(stack));
})();

// For the remaining tests, collect the Callsite objects instead of just a
// string:
Error.prepareStackTrace = function(error, frames) {
  return frames;
};

(function testStackFrames() {
  module.exports.main();

  verifyStack(stack, [
      // isWasm           function   line  pos        file
      [   false,           "STACK",    39,   0, "stack.js"],
      [    true,            "main",     1,   1,       null],
      [   false, "testStackFrames",    87,   0, "stack.js"],
      [   false,              null,    96,   0, "stack.js"]
  ]);
})();

(function testWasmUnreachable() {
  try {
    module.exports.exec_unreachable();
    fail("expected wasm exception");
  } catch (e) {
    assertContains("unreachable", e.message);
    verifyStack(e.stack, [
        // isWasm               function   line  pos        file
        [    true,    "exec_unreachable",    2,    1,       null],
        [   false, "testWasmUnreachable",  100,    0, "stack.js"],
        [   false,                  null,  111,    0, "stack.js"]
    ]);
  }
})();

(function testWasmMemOutOfBounds() {
  try {
    module.exports.call_mem_out_of_bounds();
    fail("expected wasm exception");
  } catch (e) {
    assertContains("out of bounds", e.message);
    verifyStack(e.stack, [
        // isWasm                  function   line  pos        file
        [    true,                       "",     3,   3,       null],
        [    true, "call_mem_out_of_bounds",     4,   1,       null],
        [   false, "testWasmMemOutOfBounds",   115,   0, "stack.js"],
        [   false,                     null,   127,   0, "stack.js"]
    ]);
  }
})();


(function testStackOverflow() {
  print("testStackOverflow");
  var builder = new WasmModuleBuilder();

  var sig_index = builder.addType(kSig_v_v);
  builder.addFunction("recursion", sig_index)
    .addBody([
      kExprI32Const, 0,
      kExprCallIndirect, sig_index
    ])
    .exportFunc()
  builder.appendToTable([0]);

  try {
    builder.instantiate().exports.recursion();
    fail("expected wasm exception");
  } catch (e) {
    assertEquals("Maximum call stack size exceeded", e.message, "trap reason");
  }
})();
