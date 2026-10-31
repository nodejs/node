// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that console.table argument wrapping stops after the column ' +
    'setup terminated execution.');

const expression = `
  Object.defineProperty(Array.prototype, '0', {
    set(v) {
      Object.defineProperty(Error.prototype, 'name', {
        get() {
          console.log('FAIL: ran after termination');
          return 'X';
        },
        configurable: true,
      });
      debugger;
      for (;;) {}
    },
    configurable: true,
  });
  console.table(new Error('e'), 'col');
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.map(({value, description}) => value ?? description).join(' ')}`);
  });

  // The Array.prototype[0] setter runs while console.table builds the column
  // array; it pauses and the test terminates execution there. Afterwards
  // nothing must call into V8 again, in particular the Error.prototype.name
  // getter must not run.
  const paused = Protocol.Debugger.oncePaused();
  const evaluated = Protocol.Runtime.evaluate({expression});
  await paused;
  const terminated = Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  await terminated;
  InspectorTest.logMessage((await evaluated).error ?? 'evaluate finished');

  InspectorTest.completeTest();
})();
