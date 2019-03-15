// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

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
      toString = "wasm-function[" + exp[2] + "]:" + exp[3];
      if (exp[1] !== null) toString = exp[1] + " (" + toString + ")";
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

builder.addMemory(0, 1, false);

builder.addImport("mod", "func", kSig_v_v);

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

// Call the mem_out_of_bounds function, in order to have two wasm stack frames.
builder.addFunction("call_mem_out_of_bounds", kSig_i_v)
  .addBody([kExprCallFunction, mem_oob_func.index])
  .exportAs("call_mem_out_of_bounds");

var module = builder.instantiate({mod: {func: STACK}});

(function testSimpleStack() {
  var expected_string = 'Error\n' +
      // The line numbers below will change as this test gains / loses lines..
      '    at STACK (stack.js:38:11)\n' +            // --
      '    at main (wasm-function[1]:1)\n' +         // --
      '    at testSimpleStack (stack.js:77:18)\n' +  // --
      '    at stack.js:79:3';                        // --

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
      [   false,           "STACK",    38,   0, "stack.js"],
      [    true,            "main",     1,   1,       null],
      [   false, "testStackFrames",    88,   0, "stack.js"],
      [   false,              null,    97,   0, "stack.js"]
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
        [   false, "testWasmUnreachable",  101,    0, "stack.js"],
        [   false,                  null,  112,    0, "stack.js"]
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
        [    true,                     null,     3,   3,       null],
        [    true, "call_mem_out_of_bounds",     4,   1,       null],
        [   false, "testWasmMemOutOfBounds",   116,   0, "stack.js"],
        [   false,                     null,   128,   0, "stack.js"]
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
      kExprCallIndirect, sig_index, kTableZero
    ])
    .exportFunc();
  builder.appendToTable([0]);

  try {
    builder.instantiate().exports.recursion();
    fail("expected wasm exception");
  } catch (e) {
    assertEquals("Maximum call stack size exceeded", e.message, "trap reason");
    assertTrue(e.stack.length >= 4, "expected at least 4 stack entries");
    verifyStack(e.stack.splice(0, 4), [
        // isWasm     function  line  pos  file
        [    true, "recursion",    0,   0, null],
        [    true, "recursion",    0,   3, null],
        [    true, "recursion",    0,   3, null],
        [    true, "recursion",    0,   3, null]
    ]);
  }
})();

(function testBigOffset() {
  print('testBigOffset');
  var builder = new WasmModuleBuilder();

  let body = [kExprI32Const, 0, kExprI32Add];
  while (body.length <= 65536) body = body.concat(body);
  body.unshift(kExprI32Const, 0);
  body.push(kExprUnreachable);
  let unreachable_pos = body.length - 1;

  builder.addFunction('main', kSig_v_v).addBody(body).exportFunc();

  try {
    builder.instantiate().exports.main();
    fail('expected wasm exception');
  } catch (e) {
    assertEquals('unreachable', e.message, 'trap reason');
    verifyStack(e.stack, [
      // isWasm, function, line, pos, file
      [true, 'main', 0, unreachable_pos + 1, null],  // -
      [false, 'testBigOffset', 172, 0, 'stack.js'],  //-
      [false, null, 183, 0, 'stack.js']
    ]);
  }
})();
