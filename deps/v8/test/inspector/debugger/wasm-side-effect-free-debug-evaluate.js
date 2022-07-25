// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test side-effect-free evaluation of WebAssembly APIs');

contextGroup.addScript(`
var EMPTY_WASM_MODULE_BYTES = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
var someGlobalGlobal = new WebAssembly.Global({value: "i64"}, 42n);
var someGlobalMemory = new WebAssembly.Memory({initial: 1});
var someGlobalModule = new WebAssembly.Module(EMPTY_WASM_MODULE_BYTES);
var someGlobalInstance = new WebAssembly.Instance(someGlobalModule);
var someGlobalTable = new WebAssembly.Table({element: 'anyfunc', initial: 1});
someGlobalTable.set(0, x => x);
`, 0, 0, 'foo.js');

async function check(expression) {
  const {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({expression, throwOnSideEffect: true});
  InspectorTest.log(`${expression}: ${exceptionDetails ? 'throws' : 'ok'}`);
}

InspectorTest.runAsyncTestSuite([
  async function testWebAssemblyGlobal() {
    await Protocol.Runtime.enable();
    await check('someGlobalGlobal.value');
    await check('someGlobalGlobal.valueOf()');
    await check('new WebAssembly.Global({value: "f32", mutable: true}, 3.14)');
    await check('new WebAssembly.Global({value: "f32", mutable: false}, 3.14)');
    await check('new WebAssembly.Global({value: "f32", mutable: true}, 3.14).value');
    await check('new WebAssembly.Global({value: "f32", mutable: true}, 3.14).valueOf()');
    await Protocol.Runtime.disable();
  },

  async function testWebAssemblyInstance() {
    await Protocol.Runtime.enable();
    await check('someGlobalInstance.exports');
    await check('new WebAssembly.Instance(someGlobalModule)');
    await check('new WebAssembly.Instance(someGlobalModule).exports');
    await Protocol.Runtime.disable();
  },

  async function testWebAssemblyMemory() {
    await Protocol.Runtime.enable();
    await check('someGlobalMemory.buffer');
    await check('new WebAssembly.Memory({initial: 1})');
    await check('new WebAssembly.Memory({initial: 1}).buffer');
    await Protocol.Runtime.disable();
  },

  async function testWebAssemblyModule() {
    await Protocol.Runtime.enable();
    await check('WebAssembly.Module.customSections(someGlobalModule, ".debug_info")');
    await check('WebAssembly.Module.exports(someGlobalModule)');
    await check('WebAssembly.Module.imports(someGlobalModule)');
    await check('new WebAssembly.Module(EMPTY_WASM_MODULE_BYTES)');
    await check('WebAssembly.Module.customSections(new WebAssembly.Module(EMPTY_WASM_MODULE_BYTES), ".debug_info")');
    await check('WebAssembly.Module.exports(new WebAssembly.Module(EMPTY_WASM_MODULE_BYTES))');
    await check('WebAssembly.Module.imports(new WebAssembly.Module(EMPTY_WASM_MODULE_BYTES))');
    await Protocol.Runtime.disable();
  },

  async function testWebAssemblyTable() {
    await Protocol.Runtime.enable();
    await check('someGlobalTable.get(0)');
    await check('new WebAssembly.Table({element: "anyfunc", initial: 1})');
    await check('new WebAssembly.Table({element: "anyfunc", initial: 1}).get(0)');
    await Protocol.Runtime.disable();
  }
]);
