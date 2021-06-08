// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through wasm scripts.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();

const func_a =
    builder.addFunction('wasm_A', kSig_v_v).addBody([kExprNop, kExprNop]);

// wasm_B calls wasm_A <param0> times.
const func_b = builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmVoid,                // while
        kExprLocalGet, 0,                  // -
        kExprIf, kWasmVoid,                // if <param0> != 0
          kExprLocalGet, 0,                // -
          kExprI32Const, 1,                // -
          kExprI32Sub,                     // -
          kExprLocalSet, 0,                // decrease <param0>
          kExprCallFunction, func_a.index, // -
          kExprBr, 1,                      // continue
          kExprEnd,                        // -
        kExprEnd,                          // break
      // clang-format on
    ])
    .exportAs('main');

const module_bytes = builder.toArray();

const getResult = msg => msg.result || InspectorTest.logMessage(msg);

function setBreakpoint(offset, scriptId, scriptUrl) {
  InspectorTest.log(
      'Setting breakpoint at offset ' + offset + ' on script ' + scriptUrl);
  return Protocol.Debugger
      .setBreakpoint(
          {'location': {'scriptId': scriptId, 'lineNumber': 0, 'columnNumber': offset}})
      .then(getResult);
}

// Only set breakpoints during the first loop iteration.
var first_iteration = true;

Protocol.Debugger.onPaused(async msg => {
  let loc = msg.params.callFrames[0].location;
  InspectorTest.log('Paused:');
  await session.logSourceLocation(loc);
  InspectorTest.log('Scope:');
  for (var frame of msg.params.callFrames) {
    var functionName = frame.functionName || '(anonymous)';
    var lineNumber = frame.location.lineNumber;
    var columnNumber = frame.location.columnNumber;
    InspectorTest.log(`at ${functionName} (${lineNumber}:${columnNumber}):`);
    if (!/^wasm/.test(frame.url)) {
      InspectorTest.log('   -- skipped');
      continue;
    }
    for (var scope of frame.scopeChain) {
      InspectorTest.logObject(' - scope (' + scope.type + '):');
      var properties = await Protocol.Runtime.getProperties(
          {'objectId': scope.object.objectId});
      await WasmInspectorTest.dumpScopeProperties(properties);
    }
  }

  if (first_iteration && loc.columnNumber == func_a.body_offset) {
    // Check that setting breakpoints on active instances of A and B takes
    // effect immediately.
    setBreakpoint(func_a.body_offset + 1, loc.scriptId, frame.url);
    // All of the following breakpoints are in reachable code, except offset 17.
    for (offset of [18, 17, 11, 10, 8, 6, 2, 4]) {
      setBreakpoint(func_b.body_offset + offset, loc.scriptId, frame.url);
    }
    first_iteration = false;
  }

  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiating.');
    // Spawn asynchronously:
    WasmInspectorTest.instantiate(module_bytes);
    InspectorTest.log(
        'Waiting for wasm script (ignoring first non-wasm script).');
    // Ignore javascript and full module wasm script, get scripts for functions.
    const [, {params: wasm_script}] = await Protocol.Debugger.onceScriptParsed(2);
    // Set a breakpoint in function A at offset 0. When the debugger hits this
    // breakpoint, new ones will be added.
    await setBreakpoint(func_a.body_offset, wasm_script.scriptId, wasm_script.url);
    InspectorTest.log('Calling main(4)');
    await WasmInspectorTest.evalWithUrl('instance.exports.main(4)', 'runWasm');
    InspectorTest.log('exports.main returned!');
  }
]);
