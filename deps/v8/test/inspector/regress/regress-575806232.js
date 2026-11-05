// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that console argument wrapping stops after a custom formatter ' +
    'terminated execution.');

const expression = `
  const first = {};
  globalThis.devtoolsFormatters = [{
    header(object) {
      if (object === first) {
        debugger;
        for (;;) {}
      }
      return null;
    },
    hasBody() { return false; },
  }];
  console.log(first, Object.create(null));
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});

  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.length} argument(s)`);
  });

  // The header formatter for the first console argument pauses; the test
  // terminates execution there. Afterwards nothing must call into V8 again,
  // in particular the second console argument must not be wrapped.
  const paused = Protocol.Debugger.oncePaused();
  const evaluated = Protocol.Runtime.evaluate({expression});
  await paused;
  const terminated = Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  await terminated;
  InspectorTest.logMessage((await evaluated).error ?? 'evaluate finished');

  InspectorTest.completeTest();
})();
