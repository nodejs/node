// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.load = utils.load.bind(utils);
InspectorTest.load('test/mjsunit/wasm/wasm-module-builder.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests frame inspection with wasm frames.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();
builder.addImport('mod', 'func', kSig_v_v);
builder.addFunction('wasm_1', kSig_v_v)
    .addBody([kExprNop, kExprCallFunction, 2])
    .exportAs('main');
builder.addFunction('wasm_2', kSig_v_v).addBody([kExprCallFunction, 0]);

const module_bytes = builder.toArray();

const source = `
function call_debugger() {
  debugger;
}

let module;
function instantiate(bytes) {
  const buffer = new Uint8Array(bytes).buffer;
  module = new WebAssembly.Instance(new WebAssembly.Module(buffer), {
    mod: {func: call_debugger}
  });
}

function testFrameInspection() {
  module.exports.main();
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let wasmScriptPromise = new Promise(resolve => {
    Protocol.Debugger.onScriptParsed(({params}) => {
      if (params.url.startsWith('wasm://')) {
        resolve(params);
      }
    });
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await Protocol.Runtime.evaluate(
      {expression: `instantiate(${JSON.stringify(module_bytes)})`});

  await wasmScriptPromise;

  const expected_frames = [
    // func-name, wasm, col
    ['call_debugger', false], ['$wasm_2', true, 60], ['$wasm_1', true, 55],
    ['testFrameInspection', false], ['', false]
  ];

  let breakCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    breakCount++;
    InspectorTest.log('Paused, checking call frames:');
    if (callFrames.length !== expected_frames.length) {
      InspectorTest.log(`Expected ${expected_frames.length} frames, got ${
          callFrames.length}`);
    }
    for (let i = 0; i < callFrames.length; i++) {
      let frame = callFrames[i];
      let url = session.getCallFrameUrl(frame);
      let isWasm = url.startsWith('wasm://');
      let [expectedName, expectedWasm, expectedCol] = expected_frames[i];
      if (frame.functionName !== expectedName) {
        InspectorTest.log(`frame ${i} name mismatch: expected ${
            expectedName}, got ${frame.functionName}`);
      }
      if (isWasm !== expectedWasm) {
        InspectorTest.log(`frame ${i} isWasm mismatch: expected ${
            expectedWasm}, got ${isWasm}`);
      }
      InspectorTest.log(`frame ${i}: ${frame.functionName} (wasm: ${isWasm})`);
      if (isWasm) {
        if (frame.location.columnNumber !== expectedCol) {
          InspectorTest.log(`frame ${i} column mismatch: expected ${
              expectedCol}, got ${frame.location.columnNumber}`);
        }
        InspectorTest.log(`  line: ${frame.location.lineNumber}, column: ${
            frame.location.columnNumber}`);
      }
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'testFrameInspection()'});
  InspectorTest.log(`break_count: ${breakCount}`);
  InspectorTest.completeTest();
})();
