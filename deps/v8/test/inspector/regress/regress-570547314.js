// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/570547314.');

const builder = new WasmModuleBuilder();
const func_a = builder.addFunction('a', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI32Add,  // Breakpoint 1 (offset 5)
      kExprI32Const, 2,
      kExprI32Add,  // Breakpoint 2 (offset 8)
    ])
    .exportFunc();
const func_b = builder.addFunction('b', kSig_v_v)
    .addBody([
      kExprNop,  // Breakpoint 3 (offset 1)
    ])
    .exportFunc();

const module_bytes = builder.toArray();

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    WasmInspectorTest.instantiate(module_bytes);
    const [, {params: wasm_script}] =
        await Protocol.Debugger.onceScriptParsed(2);
    const scriptId = wasm_script.scriptId;

    // Test 1: Removing a breakpoint in `a` while a younger Wasm frame `b` is on
    // top of the stack.
    await Protocol.Runtime.evaluate({
      expression: `
        globalThis.onBreakA1 = function() {
          instance.exports.b();
          return false;
        };
      `,
    });
    const {result: {breakpointId: bp_a1}} =
        await Protocol.Debugger.setBreakpoint({
          location: {
            scriptId,
            lineNumber: 0,
            columnNumber: func_a.body_offset + 4,
          },
          condition: 'onBreakA1()',
        });
    const {result: {breakpointId: bp_b}} =
        await Protocol.Debugger.setBreakpoint({
          location: {
            scriptId,
            lineNumber: 0,
            columnNumber: func_b.body_offset,
          },
        });

    const evalA = Protocol.Runtime.evaluate({
      expression: 'instance.exports.a(10)',
    });
    await Protocol.Debugger.oncePaused();

    await Protocol.Debugger.removeBreakpoint({breakpointId: bp_a1});
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp_b});
    await Protocol.Debugger.resume();
    const {result: {result: res1}} = await evalA;
    InspectorTest.log(`Test 1 result: ${res1.value}`);

    // Test 2: Two recursive activations of `a` paused at different breakpoints
    // when both breakpoints are removed.
    await Protocol.Runtime.evaluate({
      expression: `
        globalThis.depth = 0;
        globalThis.innerResult = 0;
        globalThis.onBreakFirst = function() {
          if (globalThis.depth === 0) {
            globalThis.depth++;
            globalThis.innerResult = instance.exports.a(20);
          }
          return false;
        };
      `,
    });
    const {result: {breakpointId: bp_a_first}} =
        await Protocol.Debugger.setBreakpoint({
          location: {
            scriptId,
            lineNumber: 0,
            columnNumber: func_a.body_offset + 4,
          },
          condition: 'onBreakFirst()',
        });
    const {result: {breakpointId: bp_a_second}} =
        await Protocol.Debugger.setBreakpoint({
          location: {
            scriptId,
            lineNumber: 0,
            columnNumber: func_a.body_offset + 7,
          },
        });

    const evalOuterA = Protocol.Runtime.evaluate({
      expression: 'instance.exports.a(10)',
    });
    await Protocol.Debugger.oncePaused();

    await Protocol.Debugger.removeBreakpoint({breakpointId: bp_a_first});
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp_a_second});
    await Protocol.Debugger.resume();
    const {result: {result: resOuter}} = await evalOuterA;
    const {result: {result: resInner}} = await Protocol.Runtime.evaluate({
      expression: 'globalThis.innerResult',
    });
    InspectorTest.log(`Test 2 inner result: ${resInner.value}`);
    InspectorTest.log(`Test 2 outer result: ${resOuter.value}`);
  },
]);
