// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(v8:10266): Figure out why this fails on tsan with --always-opt.
// Flags: --no-always-opt --no-turbo-inline-js-wasm-calls

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that console profiles contain wasm function names.');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

// Build module bytes, including a sentinel such that the module will not be
// reused from the cache.
let sentinel = 0;
function buildModuleBytes() {
  ++sentinel;
  InspectorTest.log(`Building wasm module with sentinel ${sentinel}.`);
  // Add fibonacci function, calling back and forth between JS and Wasm to also
  // check for the occurrence of the wrappers.
  var builder = new WasmModuleBuilder();
  const imp_index = builder.addImport('q', 'f', kSig_i_i);
  builder.addFunction('fib', kSig_i_i)
      .addBody([
        ...wasmI32Const(sentinel), kExprDrop,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprI32Const, 2,
        kExprI32LeS,  // i < 2 ?
        kExprBrIf, 0, // --> return i
        kExprI32Const, 1, kExprI32Sub,  // i - 1
        kExprCallFunction, imp_index, // imp(i - 1)
        kExprLocalGet, 0, kExprI32Const, 2, kExprI32Sub,  // i - 2
        kExprCallFunction, imp_index, // imp(i - 2)
        kExprI32Add
      ])
      .exportFunc();
  return builder.toArray();
}

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

let found_good_profile = false;
let found_wasm_script_id;
let wasm_position;
let finished_profiles = 0;
Protocol.Profiler.onConsoleProfileFinished(e => {
  ++finished_profiles;
  let nodes = e.params.profile.nodes;
  let function_names = nodes.map(n => n.callFrame.functionName);
  // Enable this line for debugging:
  // InspectorTest.log(function_names.join(', '));
  // Check for at least one full cycle of
  // fib -> wasm-to-js -> imp -> js-to-wasm -> fib.
  // There are two different kinds of js-to-wasm-wrappers, so there are two
  // possible positive traces.
  const expected = [
    ['fib'], ['wasm-to-js:i:i'], ['imp'],
    ['GenericJSToWasmWrapper', 'js-to-wasm:i:i'], ['fib']
  ];
  for (let i = 0; i <= function_names.length - expected.length; ++i) {
    if (expected.every((val, idx) => val.includes(function_names[i + idx]))) {
      found_good_profile = true;
      let wasm_frame = nodes[i].callFrame;
      found_wasm_script_id = wasm_frame.scriptId != 0;
      wasm_position = `${wasm_frame.url}@${wasm_frame.lineNumber}:${
          wasm_frame.columnNumber}`;
    }
  }
});

async function runFibUntilProfileFound() {
  InspectorTest.log(
      'Running fib with increasing input until it shows up in the profile.');
  found_good_profile = false;
  finished_profiles = 0;
  for (let i = 1; !found_good_profile; ++i) {
    checkError(await Protocol.Runtime.evaluate(
        {expression: 'console.profile(\'profile\');'}));
    checkError(await Protocol.Runtime.evaluate(
        {expression: 'globalThis.instance.exports.fib(' + i + ');'}));
    checkError(await Protocol.Runtime.evaluate(
        {expression: 'console.profileEnd(\'profile\');'}));
    if (finished_profiles != i) {
      InspectorTest.log(
          'Missing consoleProfileFinished message (expected ' + i + ', got ' +
          finished_profiles + ')');
    }
  }
  InspectorTest.log('Found expected functions in profile.');
  InspectorTest.log(
      'Wasm script id is ' + (found_wasm_script_id ? 'set.' : 'NOT SET.'));
  InspectorTest.log('Wasm position: ' + wasm_position);
}

async function compileWasm() {
  InspectorTest.log('Compiling wasm.');
  checkError(await Protocol.Runtime.evaluate({
    expression: `globalThis.instance = (${compile})(${
        JSON.stringify(buildModuleBytes())});`
  }));
}

async function testEnableProfilerEarly() {
  InspectorTest.log(arguments.callee.name);
  checkError(await Protocol.Profiler.enable());
  checkError(await Protocol.Profiler.start());
  await compileWasm();
  await runFibUntilProfileFound();
  checkError(await Protocol.Profiler.disable());
}

async function testEnableProfilerLate() {
  InspectorTest.log(arguments.callee.name);
  await compileWasm();
  checkError(await Protocol.Profiler.enable());
  checkError(await Protocol.Profiler.start());
  await runFibUntilProfileFound();
  checkError(await Protocol.Profiler.disable());
}

async function testEnableProfilerAfterDebugger() {
  InspectorTest.log(arguments.callee.name);
  checkError(await Protocol.Debugger.enable());
  await compileWasm();
  checkError(await Protocol.Profiler.enable());
  checkError(await Protocol.Profiler.start());
  await runFibUntilProfileFound();
  checkError(await Protocol.Profiler.disable());
  checkError(await Protocol.Debugger.disable());
}

async function testEnableProfilerBeforeDebugger() {
  InspectorTest.log(arguments.callee.name);
  await compileWasm();
  await Protocol.Profiler.enable();
  await Protocol.Debugger.enable();
  checkError(await Protocol.Profiler.start());
  await runFibUntilProfileFound();
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
