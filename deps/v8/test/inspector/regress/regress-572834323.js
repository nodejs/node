// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/572834323.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setInstrumentationBreakpoint({
    instrumentation: 'beforeScriptExecution',
  });

  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: `
const builder = new WasmModuleBuilder();
builder.addImport('m', 'f', kSig_v_v);
const module = builder.toModule();
new WebAssembly.Instance(module, {m: {f: 123}});
`,
    disableBreaks: true,
  }));
  InspectorTest.completeTest();
})();
