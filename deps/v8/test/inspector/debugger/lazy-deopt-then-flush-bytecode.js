// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --stress-flush-code
// Flags: --expose-gc --flush-bytecode

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests lazy deopt followed by code flush and side-effect free evaluation.');

const source = `
function foo() {
  return 44;
}

function f() {
  debugger;
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    // Optimize foo.
    await Protocol.Runtime.evaluate({
      expression: `
        %PrepareFunctionForOptimization(foo);
        %OptimizeFunctionOnNextCall(foo);
        foo();
      `
    });

    const {result: {result: {value: isOpt}}} = await Protocol.Runtime.evaluate(
        {expression: 'Boolean(%GetOptimizationStatus(foo) & (1 << 3))'});
    InspectorTest.log('foo is optimized: ' + isOpt);

    // Lazily deopt foo, which marks the code for deoptimization and invalidates
    // the DeoptimizationData, but doesn't unlink the optimized code entry in
    // foo's JSFunction.
    await Protocol.Runtime.evaluate({expression: '%DeoptimizeFunction(foo)'});

    // Run the GC. Since the DeoptimizationData is now dead, the bytecode
    // associated with the optimized code is free to be flushed, which also
    // free's the feedback vector meta-data.
    await Protocol.Runtime.evaluate({expression: 'gc()'});

    // Execute foo with side-effect checks, which causes the debugger to call
    // DeoptimizeFunction on foo. Even though the code is already marked for
    // deoptimization, this will try to unlink the optimized code from the
    // feedback vector, which will fail due to the feedback meta-data being
    // flushed. The deoptimizer should call JSFunction::ResetIfBytecodeFlushed
    // before trying to do this, which will clear the whole feedback vector and
    // reset the JSFunction's code entry field to CompileLazy.
    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'foo()',
      throwOnSideEffect: true
    });
    InspectorTest.log(
        'evaluateOnCallFrame result: ' + evalResult.result.result.value);

    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
