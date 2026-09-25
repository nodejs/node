// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start("Check that throwing breakpoint conditions report exceptions via Runtime.exceptionThrown.");

contextGroup.addScript(`
function smallFnWithLogpoint(x) {
  return x + 42;
}
`, 0, 0, 'test.js');

Protocol.Runtime.onExceptionThrown(({ params: { exceptionDetails } }) => {
  const { description } = exceptionDetails.exception;
  InspectorTest.log(`Exception thrown: ${description}`);
});

async function testSyntaxError() {
  const { result: { breakpointId } } = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 3,
    url: 'test.js',
    condition: 'x ===',
  });
  await Protocol.Runtime.evaluate({ expression: 'smallFnWithLogpoint(5)' });
  await Protocol.Debugger.removeBreakpoint({ breakpointId });
}

async function testRepeatedErrorsCauseOneEventEach() {
  const { result: { breakpointId } } = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 3,
    url: 'test.js',
    condition: 'x ===',
  });
  await Protocol.Runtime.evaluate({
    expression: 'for (let i = 0; i < 5; ++i) smallFnWithLogpoint(5);' });
  await Protocol.Debugger.removeBreakpoint({ breakpointId });
}

async function testSporadicThrowing() {
  // Tests that a breakpoint condition going from throwing -> succeeding -> throwing
  // logs two events.
  const { result: { breakpointId } } = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 3,
    url: 'test.js',
    condition: 'y === 42',
  });
  // Causes a reference error as `y` is not defined.
  await Protocol.Runtime.evaluate({ expression: 'smallFnWithLogpoint(5)' });

  // Introduce y and trigger breakpoint again.
  const evalPromise = Protocol.Runtime.evaluate({ expression: 'globalThis.y = 42; smallFnWithLogpoint(5)' });
  await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused on conditional logpoint');
  await Promise.all([Protocol.Debugger.resume(), evalPromise]);

  // Delete 'y' again, trigger breakpoint and expect an exception event.
  await Protocol.Runtime.evaluate({ expression: 'delete globalThis.y; smallFnWithLogpoint(5)' });

  await Protocol.Debugger.removeBreakpoint({ breakpointId });
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    Protocol.Debugger.enable();
    Protocol.Runtime.enable();
    await Protocol.Debugger.onceScriptParsed();
  },
  testSyntaxError,
  testRepeatedErrorsCauseOneEventEach,
  testSporadicThrowing,
]);
