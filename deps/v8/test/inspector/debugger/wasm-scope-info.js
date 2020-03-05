// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Disable Liftoff to get deterministic (non-existing) scope information for
// compiled frames.
// Flags: --no-liftoff

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test retrieving scope information when pausing in wasm functions');
session.setupScriptMap();
Protocol.Debugger.enable();
Protocol.Debugger.onPaused(printPauseLocationsAndContinue);

let evaluate = code => Protocol.Runtime.evaluate({expression: code});

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
  await evaluate('instance.exports.main(4)');
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
      await dumpScopeProperties(properties);
    }
  }
  InspectorTest.log();
  Protocol.Debugger.stepOver();
}

async function instantiateWasm() {
  utils.load('test/mjsunit/wasm/wasm-module-builder.js');

  var builder = new WasmModuleBuilder();
  // Also add a global, so we have some global scope.
  builder.addGlobal(kWasmI32, true);

  // Add a function without breakpoint, to check that locals are shown
  // correctly in compiled code.
  builder.addFunction('call_func', kSig_v_i).addLocals({f32_count: 1}).addBody([
    // Set local 1 to 7.2.
    ...wasmF32Const(7.2), kExprLocalSet, 1,
    // Call function 'func', forwarding param 0.
    kExprLocalGet, 0, kExprCallFunction, 1
  ]).exportAs('main');

  // A second function which will be stepped through.
  let func = builder.addFunction('func', kSig_v_i)
      .addLocals(
          {i32_count: 1, i64_count: 1, f64_count: 3},
          ['i32Arg', undefined, 'i64_local', 'unicode☼f64', '0', '0'])
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

        // Set global 0 to 15
        kExprI32Const, 15, kExprGlobalSet, 0,
      ]);

  let moduleBytes = builder.toArray();
  breakpointLocation = func.body_offset;

  function instantiate(bytes) {
    var buffer = new ArrayBuffer(bytes.length);
    var view = new Uint8Array(buffer);
    for (var i = 0; i < bytes.length; ++i) {
      view[i] = bytes[i] | 0;
    }

    var module = new WebAssembly.Module(buffer);
    return new WebAssembly.Instance(module);
  }

  InspectorTest.log('Calling instantiate function.');
  evaluate(`var instance = (${instantiate})(${JSON.stringify(moduleBytes)})`);
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

async function getScopeValues(value) {
  if (value.type != 'object') {
    InspectorTest.log('Expected object. Found:');
    InspectorTest.logObject(value);
    return;
  }

  let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
  printIfFailure(msg);
  let printProperty = elem => '"' + elem.name + '"' +
      ': ' + elem.value.value + ' (' + elem.value.type + ')';
  return msg.result.result.map(printProperty).join(', ');
}

async function dumpScopeProperties(message) {
  printIfFailure(message);
  for (var value of message.result.result) {
    var value_str = await getScopeValues(value.value);
    InspectorTest.log('   ' + value.name + ': ' + value_str);
  }
}
