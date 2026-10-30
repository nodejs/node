// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that inspect() does not swallow an exception thrown by the ' +
    '"focus" option getter, and does not continue after termination.');

const throwingExpression = `
  (function() {
    try {
      inspect(new Error('inner'), {get focus() { throw new Error('boom'); }});
      return 'no throw';
    } catch (e) {
      return 'threw ' + e.message;
    }
  })()
`;

const terminatingExpression = `
  globalThis.after = false;
  inspect(new Error('inner'), {get focus() { debugger; return true; }});
  globalThis.after = true;
`;

(async () => {
  await Protocol.Runtime.enable();

  const {result: {result: throwResult}} = await Protocol.Runtime.evaluate(
      {expression: throwingExpression, includeCommandLineAPI: true});
  InspectorTest.log(`focus getter throws: ${throwResult.value}`);

  await Protocol.Debugger.enable();
  let pauses = 0;
  Protocol.Debugger.onPaused(({params: {reason}}) => {
    InspectorTest.log(`paused: ${reason}`);
    if (++pauses === 1) Protocol.Runtime.terminateExecution();
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate(
      {expression: terminatingExpression, includeCommandLineAPI: true});
  const {result: {result: afterResult}} =
      await Protocol.Runtime.evaluate({expression: 'globalThis.after'});
  InspectorTest.log(`ran after termination: ${afterResult.value}`);

  InspectorTest.completeTest();
})();
