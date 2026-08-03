// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test conditional breakpoints in wasm.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();

const fib_body = [
  kExprLocalGet, 0,                                 // i (for br_if or i32.sub)
  kExprLocalGet, 0, kExprI32Const, 2, kExprI32LeS,  // i < 2 ?
  kExprBrIf, 0,                                     // --> return i
  kExprI32Const, 1, kExprI32Sub,                    // i - 1
  kExprCallFunction, 0,                             // fib(i - 1)
  kExprLocalGet, 0, kExprI32Const, 2, kExprI32Sub,  // i - 2
  kExprCallFunction, 0,                             // fib(i - 2)
  kExprI32Add                                       // add (and return)
];
const fib = builder.addFunction('fib', kSig_i_i).addBody(fib_body).exportFunc();

const module_bytes = builder.toArray();

const find_offset = opcode => fib.body_offset + fib_body.indexOf(opcode);

const breakpoints = [
  {loc: find_offset(kExprLocalGet), cond: 'false'},
  {loc: find_offset(kExprBrIf), cond: 'true'},
  {loc: find_offset(kExprCallFunction), cond: '$var0.value==3'}
];

Protocol.Debugger.onPaused(async msg => {
  var frames = msg.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  var frame = msg.params.callFrames[0];
  for (var scope of frame.scopeChain) {
    if (scope.type != 'local') continue;
    var properties = await Protocol.Runtime.getProperties(
        {'objectId': scope.object.objectId});
    for (var {name, value} of properties.result.result) {
      value = await WasmInspectorTest.getWasmValue(value);
      InspectorTest.log(`${name}: ${value}`);
    }
  }
  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiating.');
    // Spawn asynchronously:
    WasmInspectorTest.instantiate(module_bytes);
    InspectorTest.log('Waiting for wasm script.');
    const [, {params: wasm_script}] = await Protocol.Debugger.onceScriptParsed(2);
    InspectorTest.log(`Got wasm script: ${wasm_script.url}`);
    for (let breakpoint of breakpoints) {
      InspectorTest.log(`Setting breakpoint at offset ${breakpoint.loc}, condition "${breakpoint.cond}"`);
      InspectorTest.logMessage(await Protocol.Debugger.setBreakpoint({
        'location': {
          'scriptId': wasm_script.scriptId,
          'lineNumber': 0,
          'columnNumber': breakpoint.loc
        },
        condition: breakpoint.cond
      }));
    }
    InspectorTest.log('Calling fib(5)');
    await WasmInspectorTest.evalWithUrl('instance.exports.fib(5)', 'runWasm');
    InspectorTest.log('fib returned!');
  }
]);
