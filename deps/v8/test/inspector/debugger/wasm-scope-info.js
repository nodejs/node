// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection --experimental-wasm-simd
// SIMD in Liftoff only works with these cpu features, force them on.
// Flags: --enable-sse3 --enable-ssse3 --enable-sse4-1

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test retrieving scope information when pausing in wasm functions');
session.setupScriptMap();
Protocol.Debugger.enable();
Protocol.Debugger.onPaused(printPauseLocationsAndContinue);

let breakpointLocation = undefined;  // Will be set by {instantiateWasm}.

(async function test() {
  // Instantiate wasm and wait for three wasm scripts for the three functions.
  instantiateWasm();
  let scriptIds = await waitForWasmScripts();

  // Set a breakpoint.
  InspectorTest.log(
      'Setting breakpoint on first instruction of second function');
  let breakpoint = await Protocol.Debugger.setBreakpoint({
    'location' : {
      'scriptId' : scriptIds[0],
      'lineNumber' : 0,
      'columnNumber' : breakpointLocation
    }
  });
  printIfFailure(breakpoint);
  InspectorTest.logMessage(breakpoint.result.actualLocation);

  // Now run the wasm code.
  await WasmInspectorTest.evalWithUrl('instance.exports.main(4)', 'runWasm');
  InspectorTest.log('exports.main returned. Test finished.');
  InspectorTest.completeTest();
})();

async function printPauseLocationsAndContinue(msg) {
  let loc = msg.params.callFrames[0].location;
  InspectorTest.log('Paused:');
  await session.logSourceLocation(loc);
  InspectorTest.log('Scope:');
  for (var frame of msg.params.callFrames) {
    var isWasmFrame = /^wasm/.test(frame.url);
    var functionName = frame.functionName || '(anonymous)';
    var lineNumber = frame.location.lineNumber;
    var columnNumber = frame.location.columnNumber;
    InspectorTest.log(`at ${functionName} (${lineNumber}:${columnNumber}):`);
    for (var scope of frame.scopeChain) {
      InspectorTest.logObject(' - scope (' + scope.type + '):');
      if (!isWasmFrame && scope.type == 'global') {
        // Skip global scope for non wasm-functions.
        InspectorTest.logObject('   -- skipped globals');
        continue;
      }
      var properties = await Protocol.Runtime.getProperties(
          {'objectId': scope.object.objectId});
      await WasmInspectorTest.dumpScopeProperties(properties);
    }
  }
  InspectorTest.log();
  Protocol.Debugger.stepOver();
}

async function instantiateWasm() {
  var builder = new WasmModuleBuilder();
  // Add a global, memory and exports to populate the module scope.
  builder.addGlobal(kWasmI32, true).exportAs('exported_global');
  builder.addMemory(1,1).exportMemoryAs('exported_memory');
  builder.addTable(kWasmAnyFunc, 3).exportAs('exported_table');

  // Add a function without breakpoint, to check that locals are shown
  // correctly in compiled code.
  const main = builder.addFunction('call_func', kSig_v_i).addLocals(kWasmF32, 1)
    .addBody([
      // Set local 1 to 7.2.
      ...wasmF32Const(7.2), kExprLocalSet, 1,
      // Call function 'func', forwarding param 0.
      kExprLocalGet, 0, kExprCallFunction, 1
    ]).exportAs('main');

  // A second function which will be stepped through.
  const func = builder.addFunction('func', kSig_v_i, ['i32Arg'])
    .addLocals(kWasmI32, 1)
    .addLocals(kWasmI64, 1, ['i64_local'])
    .addLocals(kWasmF64, 3, ['unicode☼f64', '0', '0'])
    .addLocals(kWasmS128, 1)
    .addLocals(kWasmF32, 1, [''])
    .addBody([
      // Set param 0 to 11.
      kExprI32Const, 11, kExprLocalSet, 0,
      // Set local 1 to 47.
      kExprI32Const, 47, kExprLocalSet, 1,
      // Set local 2 to 0x7FFFFFFFFFFFFFFF (max i64).
      kExprI64Const, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
      kExprLocalSet, 2,
      // Set local 2 to 0x8000000000000000 (min i64).
      kExprI64Const, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f,
      kExprLocalSet, 2,
      // Set local 3 to 1/7.
      kExprI32Const, 1, kExprF64UConvertI32, kExprI32Const, 7,
      kExprF64UConvertI32, kExprF64Div, kExprLocalSet, 3,
      // Set local 6 to [23, 23, 23, 23]
      kExprI32Const, 23,
      kSimdPrefix, kExprI32x4Splat,
      kExprLocalSet, 6,
      // Set local 7 to 21
      kExprI32Const, 21, kExprF32UConvertI32,
      kExprLocalSet, 7,

      // Set global 0 to 15
      kExprI32Const, 15, kExprGlobalSet, 0,
    ]);

  // Append function to table to test function table output.
  builder.appendToTable([main.index]);

  let moduleBytes = builder.toArray();
  breakpointLocation = func.body_offset;

  function addWasmJSToTable() {
    // Create WasmJS functions to test the function tables output.
    const js_func = function js_func() { return 7; };
    const wasmjs_func = new WebAssembly.Function(
      {parameters:[], results:['i32']}, js_func);
    const wasmjs_anonymous_func = new WebAssembly.Function(
      {parameters:[], results:['i32']}, _ => 7);

    instance.exports.exported_table.set(0, wasmjs_func);
    instance.exports.exported_table.set(1, wasmjs_anonymous_func);
  }

  InspectorTest.log('Calling instantiate function.');
  await WasmInspectorTest.instantiate(moduleBytes);
  await WasmInspectorTest.evalWithUrl(`(${addWasmJSToTable})()`,
                                      'populateTable');
}

function printIfFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function waitForWasmScripts() {
  InspectorTest.log('Waiting for wasm script to be parsed.');
  let wasm_script_ids = [];
  while (wasm_script_ids.length < 1) {
    let script_msg = await Protocol.Debugger.onceScriptParsed();
    let url = script_msg.params.url;
    if (url.startsWith('wasm://')) {
      InspectorTest.log('Got wasm script!');
      wasm_script_ids.push(script_msg.params.scriptId);
    }
  }
  return wasm_script_ids;
}
