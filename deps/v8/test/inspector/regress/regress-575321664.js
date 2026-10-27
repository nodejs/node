// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom preview generation stops after execution termination.');

const expression = `
  const inner = {};
  const outer = {};
  const header = ['span', ['object', {object: inner}]];
  Object.defineProperty(header, 2, {
    get() {
      console.log('FAIL: ran after termination');
      return 'x';
    },
    enumerable: true,
    configurable: true,
  });
  globalThis.devtoolsFormatters = [{
    header(object) {
      if (object === inner) {
        debugger;
        for (;;) {}
      }
      if (object === outer) return header;
      return null;
    },
    hasBody(object) { return object === outer; },
    body(object) { return ['span', 'body']; },
  }];
  console.log(outer);
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});

  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.map(({value}) => value).join(' ')}`);
  });

  // The header formatter for `inner` is called while the custom preview for
  // `outer` is generated (via the nested object tag). It pauses there, the
  // test terminates execution, and nothing else must run afterwards.
  const paused = Protocol.Debugger.oncePaused();
  const evaluated = Protocol.Runtime.evaluate({expression});
  await paused;
  const terminated = Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  await terminated;
  InspectorTest.logMessage((await evaluated).error ?? 'evaluate finished');

  InspectorTest.completeTest();
})();
