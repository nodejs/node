// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that console methods propagate failures of label conversions.');

contextGroup.addScript(`
class CustomError extends Error {}

function test(method) {
  const labels = [
    Symbol('label'),
    {toString() { throw new CustomError(); }},
  ];
  const results = [];
  for (const label of labels) {
    try {
      // Pass an additional argument, which console.timeLog() and
      // console.timeStamp() report along with the label.
      console[method](label, {});
      results.push('no exception');
    } catch (e) {
      results.push(e.constructor.name);
    }
  }
  return \`console.\${method}: \${results.join(', ')}\`;
}
`);

const methods = [
  'count',
  'countReset',
  'profile',
  'profileEnd',
  'time',
  'timeLog',
  'timeEnd',
  'timeStamp',
];

async function testMethods() {
  for (const method of methods) {
    const {result: {result: {value}}} =
        await Protocol.Runtime.evaluate({expression: `test('${method}')`});
    InspectorTest.log(value);
  }
}

(async () => {
  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`console.${type}: ${args.map(({value}) => value)}`);
  });

  InspectorTest.log('Without Runtime domain:');
  await testMethods();

  InspectorTest.log('With Runtime domain:');
  await Protocol.Runtime.enable();
  await testMethods();

  InspectorTest.log('Terminating execution during label conversion:');
  await Protocol.Debugger.enable();
  const paused = Protocol.Debugger.oncePaused();
  const evaluation = Protocol.Runtime.evaluate({
    expression: `
      console.time();
      console.timeLog({toString() { debugger; for (;;) {} }}, {});
      console.log('FAIL: execution continued after termination');
    `
  });
  await paused;
  const terminated = Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  await terminated;
  const {error} = await evaluation;
  InspectorTest.log(error?.message);

  InspectorTest.completeTest();
})();
