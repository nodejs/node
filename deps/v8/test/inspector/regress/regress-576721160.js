// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for b/576721160.');

const source = `
function lexicalArguments() {
  let arguments = 42;
  debugger;
  return arguments;
}

function tdzArguments() {
  debugger;
  try {
    return arguments;
  } catch (e) {
    return 'threw: ' + e.message;
  }
  let arguments = 42;
}

function parameterArguments(arguments) {
  debugger;
  return arguments;
}
`;

contextGroup.addScript(source);

async function testFunction(name, ...args) {
  InspectorTest.log(`Testing ${name}...`);
  const callPromise = Protocol.Runtime.evaluate({
    expression: `${name}(${args.join(', ')})`,
  });

  const {params: {callFrames: [{callFrameId}]}} =
      await Protocol.Debugger.oncePaused();

  const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: '1',
  });
  InspectorTest.logMessage(evalResult);

  await Protocol.Debugger.resume();
  const {result: {result: {value}}} = await callPromise;
  InspectorTest.log(`Returned: ${value}`);
}

(async () => {
  await Protocol.Debugger.enable();
  await testFunction('lexicalArguments');
  await testFunction('tdzArguments');
  await testFunction('parameterArguments', 7);
  InspectorTest.completeTest();
})();
