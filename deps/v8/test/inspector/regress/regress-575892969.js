// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that the console.table column loop stops after a column getter ' +
    'terminated execution.');

function makeExpression(pauseOnAccess) {
  return `
    {
      let count = 0;
      let paused = false;
      const columns = [];
      Object.defineProperty(columns, 0, {
        enumerable: true,
        get() {
          if (++count === ${pauseOnAccess}) {
            paused = true;
            debugger;
            for (;;) {}
          }
          return 'a';
        },
      });
      Object.defineProperty(columns, 1, {
        enumerable: true,
        get() {
          if (paused) console.log('second column getter ran');
          return 'b';
        },
      });
      console.table([{a: 1, b: 2}], columns);
    }
  `;
}

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.length} argument(s)`);
  });

  // Pause on access 1 (V8ValueStringBuilder::append) and access 2
  // (InjectedScript::wrapTable). In both cases the getter for the second
  // column must not run after termination.
  for (const pauseOnAccess of [1, 2]) {
    const paused = Protocol.Debugger.oncePaused();
    const evaluated =
        Protocol.Runtime.evaluate({expression: makeExpression(pauseOnAccess)});
    await paused;
    const terminated = Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    await terminated;
    InspectorTest.logMessage((await evaluated).error ?? 'evaluate finished');
  }

  InspectorTest.completeTest();
})();
