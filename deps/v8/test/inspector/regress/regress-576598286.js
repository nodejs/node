// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --strict-termination-checks

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that CommandLineAPI values() stops reading properties once ' +
    'execution is terminated, and propagates getter exceptions.');

const nonLastThrowExpression = `
  (function() {
    try {
      const o = {
        get a() { throw new Error('first'); },
        b: 2,
      };
      return JSON.stringify(values(o));
    } catch (e) {
      return 'threw ' + e.message;
    }
  })()
`;

const lastThrowExpression = `
  (function() {
    try {
      const o = {
        a: 1,
        get b() { throw new Error('last'); },
      };
      return JSON.stringify(values(o));
    } catch (e) {
      return 'threw ' + e.message;
    }
  })()
`;

const terminatingExpression = `
  globalThis.ran = false;
  globalThis.after = false;
  const o = {
    get a() { debugger; return 1; },
    get b() { globalThis.ran = true; return 2; },
  };
  values(o);
  globalThis.after = true;
`;

(async () => {
  await Protocol.Runtime.enable();

  const {result: {result: nonLastResult}} = await Protocol.Runtime.evaluate(
      {expression: nonLastThrowExpression, includeCommandLineAPI: true});
  InspectorTest.log(`getter throws (non-last): ${nonLastResult.value}`);

  const {result: {result: lastResult}} = await Protocol.Runtime.evaluate(
      {expression: lastThrowExpression, includeCommandLineAPI: true});
  InspectorTest.log(`getter throws (last): ${lastResult.value}`);

  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(() => {
    InspectorTest.log('paused');
    Protocol.Runtime.terminateExecution();
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate(
      {expression: terminatingExpression, includeCommandLineAPI: true});

  for (const name of ['ran', 'after']) {
    const {result} = await Protocol.Runtime.evaluate({expression: name});
    InspectorTest.log(`${name}: ${result.result.value}`);
  }
  InspectorTest.completeTest();
})();
