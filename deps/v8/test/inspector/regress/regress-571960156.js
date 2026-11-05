// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/571960156.');
session.setupScriptMap();
Protocol.Runtime.enable();
Protocol.Debugger.enable();

Protocol.Debugger.onPaused(async msg => {
  for (const scope of msg.params.callFrames[0].scopeChain) {
    InspectorTest.log(`scope (${scope.type}):`);
    const properties =
        await Protocol.Runtime.getProperties({objectId: scope.object.objectId});
    await WasmInspectorTest.dumpScopeProperties(properties);
  }
  Protocol.Debugger.resume();
});

async function waitForWasmScript() {
  while (true) {
    const msg = await Protocol.Debugger.onceScriptParsed();
    if (msg.params.url.startsWith('wasm://')) return msg.params.scriptId;
  }
}

InspectorTest.runAsyncTestSuite([
  async function test() {
    const builder = new WasmModuleBuilder();
    const tag = builder.addTag(kSig_v_v);
    const sig_ref_exn = builder.addType(makeSig([], [wasmRefType(kWasmExnRef)]));
    builder.addGlobal(kWasmExnRef, true, false, [kExprRefNull, kExnRefCode]);

    const body = [
      kExprBlock, ...wasmSignedLeb(sig_ref_exn),
        kExprTryTable, kWasmVoid, 1,
        kCatchRef, tag, 0,
          kExprThrow, tag,
        kExprEnd,
        kExprUnreachable,
      kExprEnd,
      kExprLocalTee, 1,
      kExprRefAsNonNull,
      kExprNop,
      kExprDrop,
    ];
    const main = builder.addFunction('main', kSig_v_v)
        .addLocals(kWasmExnRef, 1, ['null_exn'])
        .addLocals(kWasmExnRef, 1, ['caught_exn'])
        .addBody(body)
        .exportFunc();

    const module_bytes = builder.toArray();
    const breakpoint_offset = main.body_offset + body.length - 2;

    const script_promise = waitForWasmScript();
    await WasmInspectorTest.instantiate(module_bytes);
    const scriptId = await script_promise;

    await Protocol.Debugger.setBreakpoint({
      location: {scriptId, lineNumber: 0, columnNumber: breakpoint_offset},
    });

    await WasmInspectorTest.evalWithUrl('instance.exports.main()', 'runWasm');
  }
]);
