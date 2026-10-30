// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that InjectedScript::wrapTable does not dereference null mirror');

function makeExpression(pauseOnAccess) {
  return `
    {
      let n = 0;
      let err = new Error();
      Object.setPrototypeOf(err, {
        get name() {
          if (++n === ${pauseOnAccess}) {
            debugger;
            for (;;) {}
          }
          return 'E';
        },
        get message() {
          return 'm';
        }
      });
      console.table(err);
    }
  `;
}

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.length} argument(s)`);
  });

  const pauseOnAccess = 3;
  const paused = Protocol.Debugger.oncePaused();
  const evaluated =
      Protocol.Runtime.evaluate({expression: makeExpression(pauseOnAccess)});

  await paused;
  const terminated = Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  await terminated;
  InspectorTest.logMessage((await evaluated).error ?? 'evaluate finished');

  InspectorTest.completeTest();
})();
