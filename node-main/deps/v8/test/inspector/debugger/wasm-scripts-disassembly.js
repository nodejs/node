// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

InspectorTest.log("Tests disassembling wasm scripts");

let contextGroup = new InspectorTest.ContextGroup();
let { id: sessionId, Protocol } = contextGroup.connect();
Protocol.Debugger.enable();
const scriptPromise = new Promise(resolve => {
                        Protocol.Debugger.onScriptParsed(event => {
                          if (event.params.url.startsWith('wasm://')) {
                            resolve(event.params);
                          }
                        });
                      }).then(params => loadScript(params));

async function loadScript({url, scriptId}) {
  InspectorTest.log(`Session #${sessionId}: Script parsed. URL: ${url}.`);
  ({result: {scriptSource, bytecode}} =
       await Protocol.Debugger.getScriptSource({scriptId}));
  ({result: {
      streamId,
      totalNumberOfLines,
      functionBodyOffsets,
      chunk: {lines, bytecodeOffsets}
    }} = await Protocol.Debugger.disassembleWasmModule({scriptId}));

  InspectorTest.log(`Session #${sessionId}: Source for ${url}:`);
  bytecode = InspectorTest.decodeBase64(bytecode);
  const bytes = [];
  for (let i = 0; i < bytecode.length; i++) {
    let byte = bytecode[i];
    bytes.push((byte < 0x10 ? '0x0' : '0x') + byte.toString(16) + " ");
    if ((i & 7) == 7) bytes.push(` ;; offset ${i-7}..${i}\n`);
  }
  InspectorTest.log(`bytecode:\n${bytes.join("")}`);

  InspectorTest.log(`streamid: ${streamId}`);
  InspectorTest.log(`functionBodyOffsets: ${functionBodyOffsets}`);
  InspectorTest.log(`totalNumberOfLines: ${totalNumberOfLines}`);
  InspectorTest.log(`lines: \n${lines.join("\n")}`);
  InspectorTest.log(`bytecodeOffsets: ${bytecodeOffsets}`);

  if (streamId) {
    ({result: {chunk: {lines, bytecodeOffsets}}} =
         await Protocol.Debugger.nextWasmDissassemblyChunk({streamId}));
    InspectorTest.log(`chunk #2:`);
    InspectorTest.log(`lines: \n${lines.join("\n")}`);
    InspectorTest.log(`bytecodeOffsets: ${bytecodeOffsets}`);
  }
}

const builder = new WasmModuleBuilder();
builder.addFunction('f1', kSig_i_r)
  .addLocals(kWasmI32, 1, ["xyz"])
  .addLocals(kWasmF64, 1)
  .addBody([
    kExprLocalGet, 1, kExprDrop, kExprI32Const, 42
  ]);
builder.addFunction('f2', kSig_v_v).addBody([kExprNop, kExprNop]);
builder.setName('moduleName');
const module_bytes = builder.toArray();

function testFunction(bytes) {
  // Compilation triggers registration of wasm scripts.
  new WebAssembly.Module(new Uint8Array(bytes));
}

contextGroup.addScript(testFunction.toString(), 0, 0, 'v8://test/testFunction');
contextGroup.addScript('const module_bytes = ' + JSON.stringify(module_bytes));

Protocol.Runtime
    .evaluate({
      'expression': '//# sourceURL=v8://test/runTestFunction\n' +
          'testFunction(module_bytes);'
    })
    .then(() => scriptPromise)
    .catch(err => {
      InspectorTest.log(err.stack);
    })
    .then(() => InspectorTest.completeTest());
