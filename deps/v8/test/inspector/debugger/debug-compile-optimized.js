// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that setting a breakpoint deoptimizes a function and prevents ' +
    're-optimization until cleared.');

contextGroup.addScript(
    `
function f() {}

function isOptimized(fn) {
  return (%GetOptimizationStatus(fn) & (1 << 3)) !== 0;
}

function optimizeF() {
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
  return isOptimized(f);
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(() => Protocol.Debugger.resume());

  let {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'optimizeF()'});
  InspectorTest.log(`Before breakpoint, isOptimized(f): ${result.value}`);

  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 1, columnNumber: 0, url: 'test.js'});

  ({result: {result}} =
       await Protocol.Runtime.evaluate({expression: 'isOptimized(f)'}));
  InspectorTest.log(
      `Immediately after setBreakpoint, isOptimized(f): ${result.value}`);

  ({result: {result}} =
       await Protocol.Runtime.evaluate({expression: 'optimizeF()'}));
  InspectorTest.log(
      `After optimizeF() with breakpoint, isOptimized(f): ${result.value}`);

  await Protocol.Debugger.removeBreakpoint({breakpointId});

  ({result: {result}} =
       await Protocol.Runtime.evaluate({expression: 'optimizeF()'}));
  InspectorTest.log(`After removeBreakpoint and re-optimize, isOptimized(f): ${
      result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
