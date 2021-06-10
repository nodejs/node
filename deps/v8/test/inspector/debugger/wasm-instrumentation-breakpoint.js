// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test instrumentation breakpoints in wasm.');
session.setupScriptMap();

Protocol.Debugger.onPaused(async msg => {
  let top_frame = msg.params.callFrames[0];
  let reason = msg.params.reason;
  InspectorTest.log(`Paused at ${top_frame.url} with reason "${reason}".`);
  if (!top_frame.url.startsWith('v8://test/')) {
    await session.logSourceLocation(top_frame.location);
  }
  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function testBreakInStartFunction() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    InspectorTest.logMessage(
        await Protocol.Debugger.setInstrumentationBreakpoint(
            {instrumentation: 'beforeScriptExecution'}));
    InspectorTest.log('Compiling wasm module.');
    await WasmInspectorTest.compile(builder.toArray());
    InspectorTest.log('Instantiating module.');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate');
    InspectorTest.log(
        'Instantiating a second time (should trigger no breakpoint).');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate2');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
  },

  // If we compile twice, we get two instrumentation breakpoints (which might or
  // might not be expected, but it's the current behaviour).
  async function testBreakInStartFunctionCompileTwice() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    InspectorTest.logMessage(
        await Protocol.Debugger.setInstrumentationBreakpoint(
            {instrumentation: 'beforeScriptExecution'}));
    InspectorTest.log('Instantiating module.');
    await WasmInspectorTest.instantiate(builder.toArray());
    InspectorTest.log(
        'Instantiating a second time (should trigger another breakpoint).');
    await WasmInspectorTest.instantiate(builder.toArray());
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
  },

  async function testBreakInExportedFunction() {
    const builder = new WasmModuleBuilder();
    const func =
        builder.addFunction('func', kSig_v_v).addBody([kExprNop]).exportFunc();

    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    InspectorTest.logMessage(
        await Protocol.Debugger.setInstrumentationBreakpoint(
            {instrumentation: 'beforeScriptExecution'}));
    InspectorTest.log('Instantiating wasm module.');
    await WasmInspectorTest.instantiate(builder.toArray());
    InspectorTest.log(
        'Calling exported function \'func\' (should trigger a breakpoint).');
    await WasmInspectorTest.evalWithUrl('instance.exports.func()', 'call_func');
    InspectorTest.log(
        'Calling exported function \'func\' a second time ' +
        '(should trigger no breakpoint).');
    await WasmInspectorTest.evalWithUrl('instance.exports.func()', 'call_func');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
  },

  async function testBreakOnlyWithSourceMap() {
    const builder = new WasmModuleBuilder();
    const func =
        builder.addFunction('func', kSig_v_v).addBody([kExprNop]).exportFunc();
    const bytes_no_source_map = builder.toArray();
    builder.addCustomSection('sourceMappingURL', [3, 97, 98, 99]);
    const bytes_with_source_map = builder.toArray();

    await Protocol.Debugger.enable();
    InspectorTest.log(
        'Setting instrumentation breakpoint for source maps only');
    InspectorTest.logMessage(
        await Protocol.Debugger.setInstrumentationBreakpoint(
            {instrumentation: 'beforeScriptWithSourceMapExecution'}));

    InspectorTest.log('Instantiating wasm module without source map.');
    await WasmInspectorTest.instantiate(bytes_no_source_map);
    InspectorTest.log(
        'Calling exported function \'func\' (should trigger no breakpoint).');
    await WasmInspectorTest.evalWithUrl('instance.exports.func()', 'call_func');

    InspectorTest.log('Instantiating wasm module with source map.');
    await WasmInspectorTest.instantiate(bytes_with_source_map);
    InspectorTest.log(
        'Calling exported function \'func\' (should trigger a breakpoint).');
    await WasmInspectorTest.evalWithUrl('instance.exports.func()', 'call_func');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
  },

]);
