// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that Liftoff does not merge opcodes while stepping');
session.setupScriptMap();

let builder = new WasmModuleBuilder();

// i32.eqz and br_if are usually merged, so we wouldn't see any
// update on the operand stack after i32.eqz. For debugging,
// this is disabled, so we should see the result.
let body = [kExprLocalGet, 0, kExprI32Eqz, kExprBrIf, 0];
let fun = builder.addFunction('fun', kSig_v_i).addBody(body).exportFunc();

let module_bytes = builder.toArray();

let wasm_script_id = undefined;
Protocol.Debugger.onPaused(printPauseLocationAndStep);

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Debugger.enable();
    WasmInspectorTest.instantiate(module_bytes);
    [, {params: {scriptId: wasm_script_id}}] = await Protocol.Debugger.onceScriptParsed(2);

    // Set a breakpoint at the beginning of 'fun'.
    const offset = fun.body_offset;
    InspectorTest.log(`Setting breakpoint at offset ${offset}.`);
    let bpmsg = await Protocol.Debugger.setBreakpoint({
      location: {scriptId: wasm_script_id, lineNumber: 0, columnNumber: offset}
    });

    for (let value of [0, -1, 13]) {
      await Protocol.Runtime.evaluate(
          {expression: `instance.exports.fun(${value})`});
    }
  }
]);

async function printPauseLocationAndStep(msg) {
  // If we are outside of wasm, continue.
  let loc = msg.params.callFrames[0].location;
  if (loc.scriptId != wasm_script_id) {
    Protocol.Debugger.resume();
    return;
  }

  // Inspect only the top wasm frame.
  let frame = msg.params.callFrames[0];
  let scopes = {};
  for (let scope of frame.scopeChain) {
    if (scope.type == 'module') continue;
    let scope_properties =
        await Protocol.Runtime.getProperties({objectId: scope.object.objectId});
    scopes[scope.type] = scope_properties.result.result.map(
        elem => WasmInspectorTest.getWasmValue(elem.value));
  }
  let values = scopes['local'].concat(scopes['wasm-expression-stack']).join(', ');
  InspectorTest.log(`Paused at offset ${loc.columnNumber}: [${values}]`);
  Protocol.Debugger.stepOver();
}
