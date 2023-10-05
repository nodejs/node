// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(v8:10266): Figure out why this fails on tsan with --always-turbofan.
// Flags: --no-always-turbofan --no-turbo-inline-js-wasm-calls

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

function instantiate(bytes) {
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

// Remember which sequences of functions we have seen in profiles so far. This
// is printed in the error case to aid debugging.
let seen_profiles = [];
function addSeenProfile(function_names) {
  let arrays_equal = (a, b) =>
      a.length == b.length && a.every((val, index) => val == b[index]);
  if (seen_profiles.some(a => arrays_equal(a, function_names))) return false;
  seen_profiles.push(function_names.slice());
  return true;
}

function resetGlobalData() {
  found_good_profile = false;
  finished_profiles = 0;
  seen_profiles = [];
}

Protocol.Profiler.onConsoleProfileFinished(e => {
  ++finished_profiles;
  let nodes = e.params.profile.nodes;
  let function_names = nodes.map(n => n.callFrame.functionName);
  // Check for at least one full cycle of
  // fib -> wasm-to-js -> imp -> js-to-wasm -> fib.
  // There are two different kinds of js-to-wasm-wrappers, so there are two
  // possible positive traces.
  const expected = [
    ['fib'], ['wasm-to-js', 'wasm-to-js:i:i'], ['imp'],
    ['js-to-wasm', 'js-to-wasm:i:i'], ['fib']
  ];
  if (!addSeenProfile(function_names)) return;
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
  resetGlobalData();
  const start = Date.now();
  const kTimeoutMs = 30000;
  for (let i = 1; !found_good_profile; ++i) {
    checkError(await Protocol.Runtime.evaluate(
        {expression: `console.profile('profile');`}));
    checkError(await Protocol.Runtime.evaluate(
        {expression: `globalThis.instance.exports.fib(${i});`}));
    checkError(await Protocol.Runtime.evaluate(
        {expression: `console.profileEnd('profile');`}));
    if (finished_profiles != i) {
      InspectorTest.log(
          `Missing consoleProfileFinished message (expected ${i}, got ` +
          `${finished_profiles})`);
    }
    if (Date.now() - start > kTimeoutMs) {
      InspectorTest.log('Seen profiles so far:');
      for (let profile of seen_profiles) {
        InspectorTest.log('  - ' + profile.join(" -> "));
      }
      throw new Error(
          `fib did not show up in the profile within ` +
          `${kTimeoutMs}ms (after ${i} executions)`);
    }
  }
  InspectorTest.log('Found expected functions in profile.');
  InspectorTest.log(
      `Wasm script id is ${found_wasm_script_id ? 'set.' : 'NOT SET.'}`);
  InspectorTest.log(`Wasm position: ${wasm_position}`);
}

async function compileWasm(module_bytes) {
  InspectorTest.log('Compiling wasm.');
  if (module_bytes === undefined) module_bytes = buildModuleBytes();
  checkError(await Protocol.Runtime.evaluate({
    expression: `globalThis.instance = (${instantiate})(${
        JSON.stringify(module_bytes)});`
  }));
}

InspectorTest.runAsyncTestSuite([
  async function testEnableProfilerEarly() {
    checkError(await Protocol.Profiler.enable());
    checkError(await Protocol.Profiler.start());
    await compileWasm();
    await runFibUntilProfileFound();
    checkError(await Protocol.Profiler.disable());
  },

  async function testEnableProfilerLate() {
    await compileWasm();
    checkError(await Protocol.Profiler.enable());
    checkError(await Protocol.Profiler.start());
    await runFibUntilProfileFound();
    checkError(await Protocol.Profiler.disable());
  },

  async function testEnableProfilerAfterDebugger() {
    checkError(await Protocol.Debugger.enable());
    await compileWasm();
    checkError(await Protocol.Profiler.enable());
    checkError(await Protocol.Profiler.start());
    await runFibUntilProfileFound();
    checkError(await Protocol.Profiler.disable());
    checkError(await Protocol.Debugger.disable());
  },

  async function testEnableProfilerBeforeDebugger() {
    await compileWasm();
    await Protocol.Profiler.enable();
    await Protocol.Debugger.enable();
    checkError(await Protocol.Profiler.start());
    await runFibUntilProfileFound();
    await Protocol.Debugger.disable();
    await Protocol.Profiler.disable();
  },

  async function testRunningCodeInDifferentIsolate() {
    // Do instantiate the module in the inspector isolate *and* in the debugged
    // isolate. Trigger lazy compilation in the inspector isolate then. Check
    // that we can profile in the debugged isolate later, i.e. the lazily
    // compiled code was logged there.
    let module_bytes = buildModuleBytes();
    InspectorTest.log('Instantiating in inspector isolate.');
    let instance = instantiate(module_bytes);
    InspectorTest.log('Instantiating in the debugged isolate.');
    await compileWasm(module_bytes);
    InspectorTest.log('Enabling profiler in the debugged isolate.');
    await Protocol.Profiler.enable();
    checkError(await Protocol.Profiler.start());
    InspectorTest.log('Running in the inspector isolate.');
    instance.exports.fib(26);
    InspectorTest.log('Running in the debugged isolate.');
    await runFibUntilProfileFound();
    InspectorTest.log('Disabling profiler.');
    await Protocol.Profiler.disable();
  }
]);
