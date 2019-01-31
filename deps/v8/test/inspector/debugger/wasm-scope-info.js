// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test retrieving scope information when pausing in wasm functions');
session.setupScriptMap();
Protocol.Debugger.enable();

let evaluate = code => Protocol.Runtime.evaluate({expression: code});

(async function test() {
  let scriptId = await instantiateWasm();
  await setBreakpoint(scriptId);
  printPauseLocationsAndContinue();
  await evaluate('instance.exports.main(4)');
  InspectorTest.log('exports.main returned. Test finished.');
  InspectorTest.completeTest();
})();

async function printPauseLocationsAndContinue() {
  while (true) {
    let msg = await Protocol.Debugger.oncePaused();
    let loc = msg.params.callFrames[0].location;
    InspectorTest.log('Paused:');
    await session.logSourceLocation(loc);
    await dumpScopeChainsOnPause(msg);
    Protocol.Debugger.stepOver();
  }
}

async function instantiateWasm() {
  utils.load('test/mjsunit/wasm/wasm-constants.js');
  utils.load('test/mjsunit/wasm/wasm-module-builder.js');

  var builder = new WasmModuleBuilder();

  builder.addFunction('func', kSig_v_i)
      .addLocals(
          {i32_count: 1, i64_count: 1, f64_count: 1},
          ['i32Arg', undefined, 'i64_local', 'unicode☼f64'])
      .addBody([
        // Set param 0 to 11.
        kExprI32Const, 11, kExprSetLocal, 0,
        // Set local 1 to 47.
        kExprI32Const, 47, kExprSetLocal, 1,
        // Set local 2 to 0x7FFFFFFFFFFFFFFF (max i64).
        kExprI64Const, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
        kExprSetLocal, 2,
        // Set local 2 to 0x8000000000000000 (min i64).
        kExprI64Const, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f,
        kExprSetLocal, 2,
        // Set local 3 to 1/7.
        kExprI32Const, 1, kExprF64UConvertI32, kExprI32Const, 7,
        kExprF64UConvertI32, kExprF64Div, kExprSetLocal, 3
      ])
      .exportAs('main');

  var module_bytes = builder.toArray();

  function instantiate(bytes) {
    var buffer = new ArrayBuffer(bytes.length);
    var view = new Uint8Array(buffer);
    for (var i = 0; i < bytes.length; ++i) {
      view[i] = bytes[i] | 0;
    }

    var module = new WebAssembly.Module(buffer);
    // Set global variable.
    instance = new WebAssembly.Instance(module);
  }

  InspectorTest.log('Installing code and global variable.');
  await evaluate('var instance;\n' + instantiate.toString());
  InspectorTest.log('Calling instantiate function.');
  evaluate('instantiate(' + JSON.stringify(module_bytes) + ')');
  return waitForWasmScript();
}

async function setBreakpoint(scriptId) {
  InspectorTest.log('Setting breakpoint on line 2 (first instruction)');
  let breakpoint = await Protocol.Debugger.setBreakpoint(
      {'location': {'scriptId': scriptId, 'lineNumber': 2}});
  printFailure(breakpoint);
  InspectorTest.logMessage(breakpoint.result.actualLocation);
}

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function waitForWasmScript() {
  InspectorTest.log('Waiting for wasm script to be parsed.');
  while (true) {
    let script_msg = await Protocol.Debugger.onceScriptParsed();
    let url = script_msg.params.url;
    if (!url.startsWith('wasm://')) {
      continue;
    }
    InspectorTest.log('Got wasm script!');
    return script_msg.params.scriptId;
  }
}

async function getScopeValues(value) {
  if (value.type != 'object') {
    InspectorTest.log('Expected object. Found:');
    InspectorTest.logObject(value);
    return;
  }

  let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
  printFailure(msg);
  let printProperty = elem => '"' + elem.name + '"' +
      ': ' + elem.value.value + ' (' + elem.value.type + ')';
  return msg.result.result.map(printProperty).join(', ');
}

async function dumpScopeProperties(message) {
  printFailure(message);
  for (var value of message.result.result) {
    var value_str = await getScopeValues(value.value);
    InspectorTest.log('   ' + value.name + ': ' + value_str);
  }
}

async function dumpScopeChainsOnPause(message) {
  InspectorTest.log(`Scope:`);
  for (var frame of message.params.callFrames) {
    var functionName = frame.functionName || '(anonymous)';
    var lineNumber = frame.location ? frame.location.lineNumber : frame.lineNumber;
    var columnNumber = frame.location ? frame.location.columnNumber : frame.columnNumber;
    InspectorTest.log(`at ${functionName} (${lineNumber}:${columnNumber}):`);
    for (var scope of frame.scopeChain) {
      InspectorTest.logObject(' - scope (' + scope.type + '):');
      if (scope.type == 'global') {
        InspectorTest.logObject('   -- skipped globals');
        continue;
      }
      var properties = await Protocol.Runtime.getProperties(
          {'objectId': scope.object.objectId});
      await dumpScopeProperties(properties);
    }
  }
  InspectorTest.log();
}
