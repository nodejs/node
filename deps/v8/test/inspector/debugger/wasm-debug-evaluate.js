// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --wasm-expose-debug-eval

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests wasm debug-evaluate');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

function instantiate(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  let module = new WebAssembly.Module(buffer);
  return new WebAssembly.Instance(module);
}
contextGroup.addScript(instantiate.toString());

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function getWasmScript() {
  while (true) {
    const script = await Protocol.Debugger.onceScriptParsed();
    if (script.params.url.startsWith('wasm://')) return script.params;
  }
}

async function handleDebuggerPaused(data, messageObject) {
  const topFrameId = messageObject.params.callFrames[0].callFrameId;
  const params = {callFrameId: topFrameId, evaluator: data};
  try {
    const evalResult = await Protocol.Debugger.executeWasmEvaluator(params);
    InspectorTest.log('Result: ' + evalResult.result.result.value);
  } catch (err) {
    InspectorTest.log(
        'Eval failed: ' + err + '\nGot: ' + JSON.stringify(evalResult));
  }
  await Protocol.Debugger.resume();
}

async function runTest(testName, breakLine, debuggeeBytes, snippetBytes) {
  try {
    await Protocol.Debugger.onPaused(
        handleDebuggerPaused.bind(null, snippetBytes));
    InspectorTest.log('Test: ' + testName);
    const scriptListener = getWasmScript();
    const module = JSON.stringify(debuggeeBytes);
    await Protocol.Runtime.evaluate(
        {'expression': `const instance = instantiate(${module})`});
    const script = await scriptListener;
    const msg = await Protocol.Debugger.setBreakpoint({
      'location': {
        'scriptId': script.scriptId,
        'lineNumber': 0,
        'columnNumber': breakLine
      }
    });
    printFailure(msg);
    const eval = await Protocol.Runtime.evaluate(
        {'expression': 'instance.exports.main()'});
    InspectorTest.log(
        'Expected: ' + String.fromCharCode(eval.result.result.value));
    InspectorTest.log('Finished!');
  } catch (err) {
    InspectorTest.log(err.message);
  }
}

// copied from v8
function encode64(data) {
  const BASE =
      'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  const PAD = '=';
  var ret = '';
  var leftchar = 0;
  var leftbits = 0;
  for (var i = 0; i < data.length; i++) {
    leftchar = (leftchar << 8) | data[i];
    leftbits += 8;
    while (leftbits >= 6) {
      const curr = (leftchar >> (leftbits - 6)) & 0x3f;
      leftbits -= 6;
      ret += BASE[curr];
    }
  }
  if (leftbits == 2) {
    ret += BASE[(leftchar & 3) << 4];
    ret += PAD + PAD;
  } else if (leftbits == 4) {
    ret += BASE[(leftchar & 0xf) << 2];
    ret += PAD;
  }
  return ret;
}

(async () => {
  try {
    await Protocol.Debugger.enable();

    await (async function TestGetMemory() {
      const debuggee_builder = new WasmModuleBuilder();
      debuggee_builder.addMemory(256, 256);
      const mainFunc =
          debuggee_builder.addFunction('main', kSig_i_v)
              .addBody([
                // clang-format off
                kExprI32Const, 32,
                kExprI32Const, 50,
                kExprI32StoreMem, 0, 0,
                kExprI32Const, 32,
                kExprI32LoadMem, 0, 0,
                kExprReturn
                // clang-format on
              ])
              .exportAs('main');

      const snippet_builder = new WasmModuleBuilder();
      snippet_builder.addMemory(1, 1);
      const getMemoryIdx = snippet_builder.addImport(
          'env', '__getMemory', makeSig([kWasmI32, kWasmI32, kWasmI32], []));
      const heapBase = 32;  // Just pick some position in memory
      snippet_builder.addFunction('wasm_format', kSig_i_v)
          .addBody([
            // clang-format off
            // __getMemory(32, 4, heapBase)
            kExprI32Const, 32, kExprI32Const, 4, kExprI32Const, heapBase,
            kExprCallFunction, getMemoryIdx,
            // return heapBase
            kExprI32Const, heapBase,
            kExprReturn
            // clang-format on
          ])
          .exportAs('wasm_format');

      const debuggeeModule = debuggee_builder.toArray();
      await runTest(
          'TestGetMemory', mainFunc.body_offset + 9, debuggeeModule,
          encode64(snippet_builder.toArray()));
    })();

  } catch (err) {
    InspectorTest.log(err)
  }
  InspectorTest.completeTest();
})();
