// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --debug-in-liftoff

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through wasm scripts.');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

const func_a_idx =
    builder.addFunction('wasm_A', kSig_v_v).addBody([kExprNop, kExprNop]).index;

// wasm_B calls wasm_A <param0> times.
const func_b = builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmStmt,               // while
        kExprLocalGet, 0,                 // -
        kExprIf, kWasmStmt,               // if <param0> != 0
          kExprLocalGet, 0,               // -
          kExprI32Const, 1,               // -
          kExprI32Sub,                    // -
          kExprLocalSet, 0,               // decrease <param0>
          kExprCallFunction, func_a_idx,  // -
          kExprBr, 1,                     // continue
          kExprEnd,                       // -
        kExprEnd,                         // break
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

function setBreakpoint(offset, script) {
  InspectorTest.log(
      'Setting breakpoint at offset ' + offset + ' on script ' + script.url);
  return Protocol.Debugger
      .setBreakpoint(
          {'location': {'scriptId': script.scriptId, 'lineNumber': 0, 'columnNumber': offset}})
      .then(getResult);
}

Protocol.Debugger.onPaused(pause_msg => {
  let loc = pause_msg.params.callFrames[0].location;
  if (loc.lineNumber != 0) {
    InspectorTest.log('Unexpected line number: ' + loc.lineNumber);
  }
  InspectorTest.log('Breaking on byte offset ' + loc.columnNumber);
  Protocol.Debugger.resume();
});

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
  for (offset of [11, 10, 8, 6, 2, 4]) {
    await setBreakpoint(func_b.body_offset + offset, wasm_script);
  }
  InspectorTest.log('Calling main(4)');
  await evalWithUrl('instance.exports.main(4)', 'runWasm');
  InspectorTest.log('exports.main returned!');
  InspectorTest.log('Finished!');
  InspectorTest.completeTest();
})();
