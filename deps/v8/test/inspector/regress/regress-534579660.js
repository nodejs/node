// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --liftoff --no-wasm-tier-up

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/534579660');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
function instantiateAndRun() {
  let builder = new WasmModuleBuilder();
  let array_type = builder.addArray(kWasmI32);

  // create_array() returns (ref $array_type)
  builder.addFunction('create_array', makeSig([], [wasmRefType(array_type)]))
      .addBody([kExprI32Const, 5, kGCPrefix, kExprArrayNewDefault, array_type])
      .exportFunc();

  // trigger(arr_0, arr_1, ..., arr_29)
  let num_params = 30;
  let params = Array(num_params).fill(wasmRefType(array_type));
  builder.addFunction('trigger', makeSig(params, [kWasmI32]))
      .addLocals(kWasmI32, 1)
      .addBody([
        ...Array(num_params).fill(0).map((_, i) => [kExprLocalGet, i]).flat(),
        ...wasmI32Const(0x7fffffff),  // OOB index
        kGCPrefix, kExprArrayGet, array_type, kExprLocalSet, num_params,
        ...Array(num_params - 1).fill(kExprDrop), kExprLocalGet, num_params
      ])
      .exportFunc();

  let module_bytes = builder.toArray();
  globalThis.body_offset = builder.functions[1].body_offset;

  let instance = builder.instantiate({});
  let arr = instance.exports.create_array();

  let threw = false;
  try {
    instance.exports.trigger(...Array(30).fill(arr));
  } catch (e) {
    threw = e instanceof WebAssembly.RuntimeError;
  }
  return threw;
}
`);

let breakpoint_set = false;
Protocol.Debugger.onPaused(async msg => {
  if (!breakpoint_set) {
    breakpoint_set = true;
    // The top frame is $trigger itself when paused on exception/trap.
    const triggerFrame = msg.params.callFrames[0];
    const scriptId = triggerFrame.location.scriptId;
    const columnNumber = triggerFrame.location.columnNumber;
    InspectorTest.log(
        `Paused in ${triggerFrame.functionName} at column ${columnNumber}`);

    // Set breakpoint inside trigger at the exact trapping columnNumber.
    const bpResult = await Protocol.Debugger.setBreakpoint({
      location: {scriptId, lineNumber: 0, columnNumber},
    });
    InspectorTest.log('Breakpoint set: ' + ('breakpointId' in bpResult.result));

    // Once the breakpoint is set, trigger GC while paused to verify the
    // safepoint layout.
    await Protocol.Runtime.evaluate({expression: 'gc()'});
  }
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: 'instantiateAndRun()',
    returnByValue: true,
  });
  InspectorTest.log('Threw WebAssembly.RuntimeError: ' + result.value);
  InspectorTest.log('breakpoint_set: ' + breakpoint_set);
  InspectorTest.completeTest();
})();
