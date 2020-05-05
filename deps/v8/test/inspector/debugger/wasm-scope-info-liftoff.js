// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --debug-in-liftoff

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test retrieving scope information from compiled Liftoff frames');
session.setupScriptMap();
Protocol.Debugger.enable();
Protocol.Debugger.onPaused(printPauseLocationsAndContinue);

let evaluate = code => Protocol.Runtime.evaluate({expression: code});

let breakpointLocation = -1;

(async function test() {
  // Instantiate wasm and wait for three wasm scripts for the three functions.
  instantiateWasm();
  let scriptIds = await waitForWasmScripts();

  // Set a breakpoint.
  InspectorTest.log(
      'Setting breakpoint on line 2 (first instruction) of third function');
  let breakpoint = await Protocol.Debugger.setBreakpoint(
      {'location': {'scriptId': scriptIds[0], 'lineNumber': 0, 'columnNumber': breakpointLocation}});
  printIfFailure(breakpoint);
  InspectorTest.logMessage(breakpoint.result.actualLocation);

  // Now run the wasm code.
  await evaluate('instance.exports.main(42)');
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

  // Add two functions without breakpoint, to check that locals and operand
  // stack values are shown correctly in Liftoff code.
  // Function A will have its parameter spilled to the stack, because it calls
  // function B.
  // Function B has a local with a constant value (not using a register), the
  // parameter will be held in a register.
  builder.addFunction('A (liftoff)', kSig_v_i)
      .addBody([
        // Call function 'B', forwarding param 0.
        kExprLocalGet, 0, kExprCallFunction, 1
      ])
      .exportAs('main');

  builder.addFunction('B (liftoff)', kSig_v_i)
      .addLocals(
          {i32_count: 1, f32_count: 4},
          ['i32_arg', 'i32_local', 'f32_local', '0', '0'])
      .addBody([
        // Load a parameter and a constant onto the operand stack.
        kExprLocalGet, 0, kExprI32Const, 3,
        // Set local 2 to 7.2.
        ...wasmF32Const(7.2), kExprLocalSet, 2,
        // Call function 'C', forwarding param 0.
        kExprLocalGet, 0, kExprCallFunction, 2,
        // Drop the two operand stack values.
        kExprDrop, kExprDrop
      ]);

  // A third function which will be stepped through.
  let func = builder.addFunction('C (interpreted)', kSig_v_i)
      .addLocals({i32_count: 1}, ['i32_arg', 'i32_local'])
      .addBody([
        // Set global 0 to param 0.
        kExprLocalGet, 0, kExprGlobalSet, 0,
        // Set local 1 to 47.
        kExprI32Const, 47, kExprLocalSet, 1,
      ]);

  var module_bytes = builder.toArray();
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

  InspectorTest.log('Installing instantiate code.');
  await evaluate(instantiate.toString());
  InspectorTest.log('Calling instantiate function.');
  evaluate('var instance = instantiate(' + JSON.stringify(module_bytes) + ');');
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
  if (value.type == 'object') {
    let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
    printIfFailure(msg);
    const printProperty = function(elem) {
      return `"${elem.name}": ${elem.value.value} (${elem.value.type})`;
    }
    return msg.result.result.map(printProperty).join(', ');
  }
  return value.value + ' (' + value.type + ')';
}

async function dumpScopeProperties(message) {
  printIfFailure(message);
  for (var value of message.result.result) {
    var value_str = await getScopeValues(value.value);
    InspectorTest.log('   ' + value.name + ': ' + value_str);
  }
}
