// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --allow-natives-syntax --expose-gc --liftoff
// Flags: --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let next_id = 2;
let breakpoint_set = false;
let instance;
let arr;

function receive(message) {
  let msg = JSON.parse(message);
  if (msg.error) {
    failWithMessage('Inspector error: ' + JSON.stringify(msg.error));
  }
  if (msg.id === 1) {
    // Step 2: Debugger enabled, now set pause on exceptions
    send(JSON.stringify({
      id: 10,
      method: 'Debugger.setPauseOnExceptions',
      params: {state: 'all'}
    }));
  } else if (msg.id === 10) {
    // Step 3: Pause on exceptions is set, now instantiate and run Wasm
    instantiateAndRun();
  } else if (msg.method === 'Debugger.paused') {
    if (!breakpoint_set) {
      breakpoint_set = true;
      // The top frame is $trigger itself when paused on exception/trap
      let triggerFrame = msg.params.callFrames[0];
      let scriptId = triggerFrame.location.scriptId;
      let columnNumber = triggerFrame.location.columnNumber;

      // Set breakpoint inside trigger at the exact trapping columnNumber!
      send(JSON.stringify({
        id: 2,
        method: 'Debugger.setBreakpoint',
        params: {
          location:
              {scriptId: scriptId, lineNumber: 0, columnNumber: columnNumber}
        }
      }));
    } else {
      send(JSON.stringify({id: next_id++, method: 'Debugger.resume'}));
    }
  } else if (msg.id === 2) {
    // Once the breakpoint is set, trigger GC while paused to verify the
    // safepoint layout
    gc();
    send(JSON.stringify({id: 3, method: 'Debugger.resume'}));
  }
}

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

  instance = builder.instantiate({});
  arr = instance.exports.create_array();

  assertThrows(
      () => instance.exports.trigger(...Array(30).fill(arr)),
      WebAssembly.RuntimeError);
  assertTrue(breakpoint_set);
}

// Start sequence
send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
