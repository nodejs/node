// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/840288.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

(async function test() {
  await Protocol.Debugger.enable();

  const sourcePromise = new Promise(resolve => {
    Protocol.Debugger.onScriptParsed(async ({params}) => {
      if (!params.url.startsWith('wasm://')) return;
      // The actual source doesn't matter, just don't crash.
      const {result} =
          await Protocol.Debugger.getScriptSource({scriptId: params.scriptId});
      resolve(result);
    });
  });

  await Protocol.Runtime.evaluate({
    expression: `
      var builder = new WasmModuleBuilder();
      builder.addFunction('main', kSig_v_v).addBody([]).exportFunc();
      WebAssembly.compile(builder.toBuffer());
    `,
  });

  InspectorTest.logMessage(await sourcePromise);
  InspectorTest.completeTest();
})();
