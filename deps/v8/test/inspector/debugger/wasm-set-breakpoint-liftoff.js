// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --debug-in-liftoff

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through wasm scripts.');
session.setupScriptMap();

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

const func_a =
    builder.addFunction('wasm_A', kSig_v_v).addBody([kExprNop, kExprNop]);

// wasm_B calls wasm_A <param0> times.
const func_b = builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmStmt,                // while
        kExprLocalGet, 0,                  // -
        kExprIf, kWasmStmt,                // if <param0> != 0
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

function instantiate(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  let module = new WebAssembly.Module(buffer);
  return new WebAssembly.Instance(module);
}

const getResult = msg => msg.result || InspectorTest.logMessage(msg);

const evalWithUrl = (code, url) =>
    Protocol.Runtime
        .evaluate({'expression': code + '\n//# sourceURL=v8://test/' + url})
        .then(getResult);

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
      for (var value of properties.result.result) {
        var value_str = await getScopeValues(value.value);
        InspectorTest.log('   ' + value.name + ': ' + value_str);
      }
    }
  }

  if (first_iteration && loc.columnNumber == func_a.body_offset) {
    // Check that setting breakpoints on active instances of A and B takes
    // effect immediately.
    setBreakpoint(func_a.body_offset + 1, loc.scriptId, frame.url);
    for (offset of [11, 10, 8, 6, 2, 4]) {
      setBreakpoint(func_b.body_offset + offset, loc.scriptId, frame.url);
    }
    first_iteration = false;
  }

  Protocol.Debugger.resume();
});

async function getScopeValues(value) {
  if (value.type != 'object') {
    InspectorTest.log('Expected object. Found:');
    InspectorTest.logObject(value);
    return;
  }

  let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
  let printProperty = elem => '"' + elem.name + '"' +
      ': ' + elem.value.value + ' (' + elem.value.type + ')';
  return msg.result.result.map(printProperty).join(', ');
}

(async function test() {
  await Protocol.Debugger.enable();
  InspectorTest.log('Instantiating.');
  // Spawn asynchronously:
  let instantiate_code = 'const instance = (' + instantiate + ')(' +
      JSON.stringify(module_bytes) + ');';
  evalWithUrl(instantiate_code, 'instantiate');
  InspectorTest.log(
      'Waiting for wasm script (ignoring first non-wasm script).');
  // Ignore javascript and full module wasm script, get scripts for functions.
  const [, {params: wasm_script}] = await Protocol.Debugger.onceScriptParsed(2);
  // Set a breakpoint in function A at offset 0. When the debugger hits this
  // breakpoint, new ones will be added.
  await setBreakpoint(func_a.body_offset, wasm_script.scriptId, wasm_script.url);
  InspectorTest.log('Calling main(4)');
  await evalWithUrl('instance.exports.main(4)', 'runWasm');
  InspectorTest.log('exports.main returned!');
  InspectorTest.log('Finished!');
  InspectorTest.completeTest();
})();
