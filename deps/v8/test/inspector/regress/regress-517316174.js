// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/517316174.');

const builder = new WasmModuleBuilder();
const num_params = 100;
// Use a large signature to maximize the call instruction size.
const sig_idx = builder.addType(
    makeSig(new Array(num_params).fill(kWasmI64), []));
const imp_idx = builder.addImport('m', 'js', sig_idx);
builder.addTable(kWasmFuncRef, 1);
builder.addActiveElementSegment(0, [kExprI32Const, 0], [imp_idx]);

const func_main = builder.addFunction('main', kSig_v_v)
    .addBody([
      kExprNop,  // Offset 1
      ...new Array(num_params).fill([kExprI64Const, 0]).flat(),
      kExprI32Const, 0,  // table index
      kExprCallIndirect, sig_idx, 0,
      kExprI64Const, 0,
      kExprDrop,
      kExprI64Const, 0,
      kExprDrop,
    ])
    .exportFunc();

// Calculate the offset of the call_indirect instruction.
// 1 (nop) + 100 * 2 (i64.const 0) + 2 (i32.const 0) = 203.
const call_offset = 1 + num_params * 2 + 2;

const module_bytes = builder.toArray();

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    WasmInspectorTest.instantiate(
        module_bytes, 'instance', `{
          m: {
            js: (...args) => {
              if (globalThis.recompiled) return;
              globalThis.recompiled = true;
              debugger;
            }
          }
        }`);
    const [, {params: wasm_script}] =
        await Protocol.Debugger.onceScriptParsed(2);
    const scriptId = wasm_script.scriptId;

    // Breakpoint 1: At nop (offset 1). This ensures 'UpdateReturnAddresses'
    // has some existing state to work with and triggers the buggy path.
    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId,
        lineNumber: 0,
        columnNumber: func_main.body_offset,
      },
    });

    // Breakpoint 2: Specifically at the call site.
    const {result: {breakpointId: bp_call}} =
        await Protocol.Debugger.setBreakpoint({
          location: {
            scriptId,
            lineNumber: 0,
            columnNumber: func_main.body_offset + call_offset - 1,
          },
        });

    const evalMain = Protocol.Runtime.evaluate({
      expression: 'instance.exports.main()',
    });

    // Pause 1: Breakpoint 1 at nop.
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.resume();

    // Pause 2: Breakpoint 2 at call_indirect.
    await Protocol.Debugger.oncePaused();
    await Protocol.Debugger.resume();

    // Pause 3: Inside m.js while 'main' is on the stack at the call site.
    await Protocol.Debugger.oncePaused();

    // Trigger UpdateReturnAddresses(isolate, ..., kAfterBreakpoint)
    // for the topmost Wasm frame ('main') which is currently at the call site.
    await Protocol.Debugger.removeBreakpoint({breakpointId: bp_call});

    // Set a new breakpoint to force recompilation.
    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId,
        lineNumber: 0,
        columnNumber: func_main.body_offset - 1,
      },
    });

    await Protocol.Debugger.resume();
    await evalMain;
  },
]);
