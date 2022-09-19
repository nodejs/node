// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test wasm memory names');

let func;

// No name memory.
function createModuleBytesUnnamedMemory() {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  func = builder.addFunction('main', kSig_i_i)
             .addBody([kExprI32Const, 0, kExprI32LoadMem, 0, 0])
             .exportAs('main');

  return JSON.stringify(builder.toArray());
}

// Exported memory.
function createModuleBytesExportedMemory() {
  let builder = new WasmModuleBuilder();
  var memory = builder.addMemory(1, 1);
  builder.addExportOfKind('exported_memory', kExternalMemory);
  func = builder.addFunction('main', kSig_i_i)
             .addBody([kExprI32Const, 0, kExprI32LoadMem, 0, 0])
             .exportAs('main');

  return JSON.stringify(builder.toArray());
}

// Imported memory.
function createModuleBytesImportedMemory() {
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory('module_name', 'imported_mem', 0, 1);
  func = builder.addFunction('main', kSig_i_i)
             .addBody([kExprI32Const, 0, kExprI32LoadMem, 0, 0])
             .exportAs('main');

  return JSON.stringify(builder.toArray());
}

function createInstance(moduleBytes) {
  let module = new WebAssembly.Module((new Uint8Array(moduleBytes)).buffer);
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1});
  instance =
      new WebAssembly.Instance(module, {module_name: {imported_mem: memory}});
}

async function check(moduleBytes) {
  Protocol.Runtime.evaluate({
    expression: `
    createInstance(${moduleBytes});`
  });

  InspectorTest.log('Waiting for wasm script to be parsed.');
  let scriptId;
  while (true) {
    let msg = await Protocol.Debugger.onceScriptParsed();
    if (msg.params.url.startsWith('wasm://')) {
      scriptId = msg.params.scriptId;
      break;
    }
  }

  InspectorTest.log('Setting breakpoint in wasm.');
  await Protocol.Debugger.setBreakpoint(
      {location: {scriptId, lineNumber: 0, columnNumber: func.body_offset}});

  InspectorTest.log('Running main.');
  Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});

  const {params: {callFrames: [{callFrameId}]}} =
      await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused in debugger.');
  const {result: {result: {objectId}}} =
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: `memories`});
  const {result: {result: properties}} =
      await Protocol.Runtime.getProperties({objectId});
  for (const {name} of properties) {
    InspectorTest.log(`name: ${name}`);
  }
  await Protocol.Debugger.resume();

  InspectorTest.log('Finished.');
}

contextGroup.addScript(`
  let instance;
  ${createInstance.toString()}`);

InspectorTest.runAsyncTestSuite([
  async function test() {
    Protocol.Debugger.enable();
    await check(createModuleBytesUnnamedMemory());
    await check(createModuleBytesExportedMemory());
    await check(createModuleBytesImportedMemory());
  }
]);
