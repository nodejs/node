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
  let hitBreakpoints = msg.params.hitBreakpoints;
  const url = session.getCallFrameUrl(top_frame);
  InspectorTest.log(`Paused at ${url} with reason "${reason}".`);
  if (!url.startsWith('v8://test/')) {
    await session.logSourceLocation(top_frame.location);
  }
  // Report the hit breakpoints to make sure that it is empty, as
  // instrumentation breakpoints should not be reported as normal
  // breakpoints.
  InspectorTest.log(`Hit breakpoints: ${JSON.stringify(hitBreakpoints)}`)
  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function testBreakInStartFunction() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Runtime.enable();
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
    await Protocol.Runtime.disable();
  },

  // If we compile twice, we get two instrumentation breakpoints (which might or
  // might not be expected, but it's the current behaviour).
  async function testBreakInStartFunctionCompileTwice() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Runtime.enable();
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
    await Protocol.Runtime.disable();
  },

  async function testBreakInExportedFunction() {
    const builder = new WasmModuleBuilder();
    builder.addFunction('func', kSig_v_v).addBody([kExprNop]).exportFunc();

    await Protocol.Runtime.enable();
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
    await Protocol.Runtime.disable();
  },

  async function testBreakOnlyWithSourceMap() {
    const builder = new WasmModuleBuilder();
    builder.addFunction('func', kSig_v_v).addBody([kExprNop]).exportFunc();
    const bytes_no_source_map = builder.toArray();
    builder.addCustomSection('sourceMappingURL', [3, 97, 98, 99]);
    const bytes_with_source_map = builder.toArray();

    await Protocol.Runtime.enable();
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
    await Protocol.Runtime.disable();
  },

  async function testRemoveBeforeCompile() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    const addMsg = await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'})
    InspectorTest.logMessage(addMsg);
    InspectorTest.log('Remove instrumentation breakpoint..');
    await Protocol.Debugger.removeBreakpoint(
        {breakpointId: addMsg.result.breakpointId});
    InspectorTest.log('Compiling wasm module.');
    await WasmInspectorTest.compile(builder.toArray());
    InspectorTest.log('Instantiating module should not trigger a break.');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testRemoveBeforeInstantiate() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    const addMsg = await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'})
    InspectorTest.logMessage(addMsg);
    InspectorTest.log('Compiling wasm module.');
    await WasmInspectorTest.compile(builder.toArray());
    InspectorTest.log('Remove instrumentation breakpoint..');
    await Protocol.Debugger.removeBreakpoint(
        {breakpointId: addMsg.result.breakpointId});
    InspectorTest.log('Instantiating module should not trigger a break.');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testRemoveAfterOnePause() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    const addMsg = await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'})
    InspectorTest.logMessage(addMsg);
    InspectorTest.log('Compiling wasm module.');
    await WasmInspectorTest.compile(builder.toArray());
    InspectorTest.log('Instantiating module should trigger a break.');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate');
    InspectorTest.log('Remove instrumentation breakpoint..');
    await Protocol.Debugger.removeBreakpoint(
        {breakpointId: addMsg.result.breakpointId});

    InspectorTest.log('Compiling another wasm module.');
    builder.addFunction('end', kSig_v_v).addBody([kExprNop]);
    await WasmInspectorTest.compile(builder.toArray());
    InspectorTest.log('Instantiating module should not trigger a break.');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testDisableEnable() {
    const builder = new WasmModuleBuilder();
    const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
    builder.addStart(start_fn.index);

    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Setting instrumentation breakpoint');
    const addMsg = await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'})
    InspectorTest.logMessage(addMsg);
    InspectorTest.log('Compiling wasm module.');
    await WasmInspectorTest.compile(builder.toArray());
    InspectorTest.log('Disable debugger..');
    await Protocol.Debugger.disable();
    InspectorTest.log('Enable debugger');
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiating module should not trigger a break.');
    await WasmInspectorTest.evalWithUrl(
        'new WebAssembly.Instance(module)', 'instantiate');
    InspectorTest.log('Done.');
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

]);
