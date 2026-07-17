// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test console profiles for asm.js.');

function compile(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; i++) {
    view[i] = bytes[i] | 0;
  }
  let module = new WebAssembly.Module(buffer);
  let fib = undefined;
  function imp(i) { return fib(i); }
  let instance = new WebAssembly.Instance(module, {q: {f: imp}});
  fib = instance.exports.fib;
  return instance;
}

function checkError(message) {
  if (!message.error) return;
  InspectorTest.log('Error: ');
  InspectorTest.logMessage(message);
  InspectorTest.completeTest();
}

// When build asm.js modules, include a sentinel such that the module will not
// be reused from the cache.
let sentinel = 0;

function AsmModule(stdlib, foreign, heap) {
  "use asm";
  function f() {
    return sentinel;
  }
  return {f: f};
}

async function compileAsmJs() {
  InspectorTest.log(`Compiling asm.js module with sentinel ${sentinel}.`);
  let code = AsmModule.toString().replace('sentinel', sentinel.toString());
  ++sentinel;
  checkError(await Protocol.Runtime.evaluate({expression: `(${code})().f()`}));
}

async function testEnableProfilerEarly() {
  InspectorTest.log(arguments.callee.name);
  checkError(await Protocol.Profiler.enable());
  checkError(await Protocol.Profiler.start());
  await compileAsmJs();
  checkError(await Protocol.Profiler.disable());
}

async function testEnableProfilerLate() {
  InspectorTest.log(arguments.callee.name);
  await compileAsmJs();
  checkError(await Protocol.Profiler.enable());
  checkError(await Protocol.Profiler.start());
  checkError(await Protocol.Profiler.disable());
}

async function testEnableProfilerAfterDebugger() {
  InspectorTest.log(arguments.callee.name);
  checkError(await Protocol.Debugger.enable());
  await compileAsmJs();
  checkError(await Protocol.Profiler.enable());
  checkError(await Protocol.Profiler.start());
  checkError(await Protocol.Profiler.disable());
  checkError(await Protocol.Debugger.disable());
}

async function testEnableProfilerBeforeDebugger() {
  InspectorTest.log(arguments.callee.name);
  await compileAsmJs();
  await Protocol.Profiler.enable();
  await Protocol.Debugger.enable();
  checkError(await Protocol.Profiler.start());
  await Protocol.Debugger.disable();
  await Protocol.Profiler.disable();
}

(async function test() {
  try {
    await testEnableProfilerEarly();
    await testEnableProfilerLate();
    await testEnableProfilerAfterDebugger();
    await testEnableProfilerBeforeDebugger();
  } catch (e) {
    InspectorTest.log('caught: ' + e);
  }
})().catch(e => InspectorTest.log('caught: ' + e))
    .finally(InspectorTest.completeTest);
