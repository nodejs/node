// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!InspectorTest.load) InspectorTest.load = utils.load.bind(utils);
InspectorTest.load('test/mjsunit/wasm/wasm-module-builder.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests setting and removing breakpoints in Wasm.');

const builder = new WasmModuleBuilder();
const body_a = [
  kExprLocalGet, 0,  // local.get i0
  kExprI32Const, 1,  // i32.const 1
  kExprI32Add        // i32.add i0 1
];
const fun_a = builder.addFunction('a', kSig_i_i).addBody(body_a).exportFunc();
const body_b = [
  kExprLocalGet,
  0,  // local.get i0
  kExprCallFunction,
  fun_a.index,  // call a
  kExprLocalGet,
  1,            // local.get i1
  kExprI32Sub,  // i32.sub a(i0) i1
  kExprCallFunction,
  fun_a.index,  // call a
];
const fun_b = builder.addFunction('b', kSig_i_ii).addBody(body_b).exportFunc();

const a_localget_offset = body_a.indexOf(kExprLocalGet);
const a_const_offset = body_a.indexOf(kExprI32Const);
const a_add_offset = body_a.indexOf(kExprI32Add);
const b_sub_offset = body_b.indexOf(kExprI32Sub);

const module_bytes = builder.toArray();

(async function test() {
  await Protocol.Debugger.enable();

  const instantiatePromise = Protocol.Runtime.evaluate({
    expression: `
      const bytes = new Uint8Array(${JSON.stringify(module_bytes)});
      const module = new WebAssembly.Module(bytes);
      var instance = new WebAssembly.Instance(module);
    `,
  });

  const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
  await instantiatePromise;
  const scriptId = wasmScript.scriptId;

  let breakpoint_a = await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId,
      lineNumber: 0,
      columnNumber: fun_a.body_offset + a_add_offset,
    },
  });
  let breakpoint_a_id = breakpoint_a.result.breakpointId;

  let breakpoint_b = await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId,
      lineNumber: 0,
      columnNumber: fun_b.body_offset + b_sub_offset,
    },
  });

  Protocol.Debugger.onPaused(async msg => {
    const frame = msg.params.callFrames[0];
    const functionName = frame.functionName;
    const columnNumber = frame.location.columnNumber;
    InspectorTest.log(`Break at ${functionName} offset ${columnNumber}`);

    if (functionName === '$b') {
      // When we stop in b, we add another breakpoint in a at offset 0 and
      // remove the existing breakpoint.
      await Protocol.Debugger.setBreakpoint({
        location: {
          scriptId,
          lineNumber: 0,
          columnNumber: fun_a.body_offset + a_localget_offset,
        },
      });
      await Protocol.Debugger.removeBreakpoint({breakpointId: breakpoint_a_id});
    } else if (
        functionName === '$a' &&
        columnNumber === fun_a.body_offset + a_localget_offset) {
      // When we stop at a at local.get, we set another breakpoint *in the same
      // function*, one instruction later (at i32.const).
      await Protocol.Debugger.setBreakpoint({
        location: {
          scriptId,
          lineNumber: 0,
          columnNumber: fun_a.body_offset + a_const_offset,
        },
      });
    }

    await Protocol.Debugger.resume();
  });

  InspectorTest.log('Running b(11).');
  await Protocol.Runtime.evaluate({expression: 'instance.exports.b(11)'});
  InspectorTest.log('Returned from wasm.');

  InspectorTest.completeTest();
})().catch(e => {
  InspectorTest.log('Uncaught: ' + e);
  InspectorTest.completeTest();
});
