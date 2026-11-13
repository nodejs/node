// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that breakpoints fire in optimized code after side-effect check.');

const source = `
function foo(x) {
  const o = {};
  o.a = x;
  return o.a;
}

function bar(x) {
  const o = {};
  o.a = x;
  return o.a;
}

function inliner(x) {
  return foo(x);
}

function isOptimized(f) {
  return Boolean(%GetOptimizationStatus(f) & (1 << 3));
}
//# sourceURL=test.js
`;

(async function main() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  await Protocol.Runtime.evaluate({expression: source});

  Protocol.Debugger.onPaused((message) => {
    let callFrame = message.params.callFrames[0];
    InspectorTest.log(
        'Paused in: ' + callFrame.functionName + ':' +
        (callFrame.location.lineNumber + 1));
    Protocol.Debugger.resume();
  });

  // 1. Perform a side-effect check on foo.
  InspectorTest.log('Evaluating foo with throwOnSideEffect: true');
  let evalResult = await Protocol.Runtime.evaluate(
      {expression: 'foo(1)', throwOnSideEffect: true});
  if (evalResult.result.exceptionDetails) {
    InspectorTest.log(
        'Unexpected exception in side-effect check: ' +
        JSON.stringify(evalResult.result.exceptionDetails));
  } else {
    InspectorTest.log(
        'Side-effect check evaluated successfully: ' +
        evalResult.result.result.value);
  }

  // 2. Optimize foo.
  InspectorTest.log('Optimizing foo');
  await Protocol.Runtime.evaluate({
    expression: `
      %PrepareFunctionForOptimization(foo);
      foo(1); foo(2);
      %OptimizeFunctionOnNextCall(foo);
      foo(3);
    `
  });

  let {result: {result: {value: fooOptimized}}} =
      await Protocol.Runtime.evaluate({expression: 'isOptimized(foo)'});
  InspectorTest.log('foo is optimized: ' + fooOptimized);

  // Optimize bar (control function, never side-effect evaluated).
  InspectorTest.log('Optimizing bar');
  await Protocol.Runtime.evaluate({
    expression: `
      %PrepareFunctionForOptimization(bar);
      bar(1); bar(2);
      %OptimizeFunctionOnNextCall(bar);
      bar(3);
    `
  });
  let {result: {result: {value: barOptimized}}} =
      await Protocol.Runtime.evaluate({expression: 'isOptimized(bar)'});
  InspectorTest.log('bar is optimized: ' + barOptimized);

  // 3. Set breakpoint in bar (control).
  InspectorTest.log('Setting breakpoint in bar');
  let bpBar = await Protocol.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 10});
  InspectorTest.log('Calling bar(4)');
  await Protocol.Runtime.evaluate({expression: 'bar(4)'});

  // 4. Set breakpoint in foo (the bug: PrepareFunctionForDebugExecution skipped
  // deopt).
  InspectorTest.log('Setting breakpoint in foo');
  let bpFoo = await Protocol.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 4});
  InspectorTest.log('Calling foo(4)');
  await Protocol.Runtime.evaluate({expression: 'foo(4)'});

  // 5. Test inlined case: optimize inliner (inlines foo).
  InspectorTest.log('Testing inlined caller');
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bpFoo.result.breakpointId});
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bpBar.result.breakpointId});

  // Another side-effect check on foo before optimizing inliner.
  await Protocol.Runtime.evaluate(
      {expression: 'foo(1)', throwOnSideEffect: true});

  await Protocol.Runtime.evaluate({
    expression: `
      %PrepareFunctionForOptimization(inliner);
      inliner(1); inliner(2);
      %OptimizeFunctionOnNextCall(inliner);
      inliner(3);
    `
  });
  let {result: {result: {value: inlinerOptimized}}} =
      await Protocol.Runtime.evaluate({expression: 'isOptimized(inliner)'});
  InspectorTest.log('inliner is optimized: ' + inlinerOptimized);

  InspectorTest.log('Setting breakpoint in foo while inlined');
  bpFoo = await Protocol.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 4});
  InspectorTest.log('Calling inliner(4)');
  await Protocol.Runtime.evaluate({expression: 'inliner(4)'});

  InspectorTest.completeTest();
})();
