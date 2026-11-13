// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.load = utils.load.bind(utils);
InspectorTest.load('test/mjsunit/wasm/wasm-module-builder.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test stepping from JS through Wasm.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();
const imp_fun = builder.addImport('imp', 'ort', kSig_i_v);
const sub_fun = builder.addFunction('sub', kSig_i_ii).addBody([
  kExprLocalGet, 0,  // local.get i0
  kExprLocalGet, 1,  // local.get i1
  kExprI32Sub        // i32.sub i0 i1
]);
builder.addFunction('main', kSig_i_v)
    .addBody([
      kExprCallFunction, imp_fun,       // call import
      kExprI32Const, 3,                 // i32.const 3
      kExprCallFunction, sub_fun.index  // call 'sub'
    ])
    .exportFunc();

const module_bytes = builder.toArray();

const source = `
function imported() {
  debugger;
  return 7;
}

let instance;
function instantiate(bytes) {
  const buffer = new Uint8Array(bytes).buffer;
  instance = new WebAssembly.Instance(new WebAssembly.Module(buffer), {
    imp: {ort: imported}
  });
}

function runMain() {
  return instance.exports.main();
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

  const expected_breaks = [
    'imported:2:2', 'imported:3:2', 'imported:3:11', '$main:0:68', '$main:0:70',
    '$sub:0:58', '$sub:0:60', '$sub:0:62', '$sub:0:63', '$main:0:72'
  ];

  let breakIndex = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let frame = callFrames[0];
    let loc = `${frame.functionName}:${frame.location.lineNumber}:${
        frame.location.columnNumber}`;
    InspectorTest.log(`Break at ${loc}`);

    if (breakIndex < expected_breaks.length) {
      let expected_loc = expected_breaks[breakIndex++];
      if (loc !== expected_loc) {
        InspectorTest.log(`Expected break at ${expected_loc}, got ${loc}`);
      }
    } else {
      InspectorTest.log(`Unexpected extra break at ${loc}`);
    }

    if (frame.functionName === '$main' && frame.location.columnNumber === 72) {
      await Protocol.Debugger.resume();
    } else {
      await Protocol.Debugger.stepInto();
    }
  });

  InspectorTest.log('Running main.');
  let res = await Protocol.Runtime.evaluate({expression: 'runMain()'});
  InspectorTest.log('Main returned.');
  InspectorTest.log('Result: ' + res.result.result.value);
  InspectorTest.completeTest();
})();
