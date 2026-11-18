// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --turbo-inlining --allow-natives-syntax

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that setting a breakpoint deoptimizes inlined functions.');

const source = `
function f1() {
  return 1;
}

function f2() {
  return 2;
}

function f3() {
  return f1();
}

function f4() {
  return 4;
}

function optimize(f) {
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}

function checkIsOptimized(f) {
  const status = %GetOptimizationStatus(f);
  // kMaybeDeopted (1 << 10) or kOptimizeMaglevOptimizesToTurbofan (1 << 19)
  if ((status & ((1 << 10) | (1 << 19))) !== 0) return true;
  return (status & (1 << 3)) !== 0;
}

function checkIsUnoptimized(f) {
  const status = %GetOptimizationStatus(f);
  if ((status & ((1 << 10) | (1 << 19))) !== 0) return true;
  const isOpt = (status & (1 << 3)) !== 0;
  // kMaglevved (1 << 12) && kOptimizeOnNextCallOptimizesToMaglev (1 << 18)
  if (isOpt && (status & (1 << 12)) !== 0 && (status & (1 << 18)) !== 0) {
    return true;
  }
  return !isOpt;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  await Protocol.Runtime.evaluate({expression: 'optimize(f1)'});
  await Protocol.Runtime.evaluate({expression: 'optimize(f2)'});
  await Protocol.Runtime.evaluate({expression: 'optimize(f3)'});

  async function checkOptimized(name) {
    const {result: {result}} = await Protocol.Runtime.evaluate(
        {expression: `checkIsOptimized(${name})`});
    InspectorTest.log(`${name} isOptimized: ${result.value}`);
  }

  async function checkUnoptimized(name) {
    const {result: {result}} = await Protocol.Runtime.evaluate(
        {expression: `!checkIsUnoptimized(${name})`});
    InspectorTest.log(`${name} isOptimized: ${result.value}`);
  }

  InspectorTest.log('Checking optimization status before breakpoint:');
  await checkOptimized('f1');
  await checkOptimized('f2');
  await checkOptimized('f3');

  // Setting break point in f1 deoptimizes f1 and f3 (which inlines f1).
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 0, url: 'test.js'});

  InspectorTest.log('Checking optimization status after breakpoint in f1:');
  await checkUnoptimized('f1');
  await checkOptimized('f2');
  await checkUnoptimized('f3');

  // We can optimize with break points set.
  InspectorTest.log('Optimizing f4 with breakpoint set:');
  await Protocol.Runtime.evaluate({expression: 'optimize(f4)'});
  await checkOptimized('f4');

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
