// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that termination inside custom preview stops remaining custom ' +
    'preview execution at call depth zero.');

const setup = `
  var log = [];
  const inner = {};
  const outer = {};
  const innerHeader = ['span', 'inner'];
  Object.defineProperty(innerHeader, 1, {
    get() {
      log.push('innerGetter');
      debugger;
      for (;;) {}
    },
    enumerable: true,
    configurable: true,
  });
  const outerHeader = ['span', ['object', {object: inner}]];
  Object.defineProperty(outerHeader, 2, {
    get() {
      log.push('FAIL: outer getter ran after termination');
      return 'x';
    },
    enumerable: true,
    configurable: true,
  });
  globalThis.devtoolsFormatters = [{
    header(object) {
      if (object === inner) {
        log.push('innerHeader');
        return innerHeader;
      }
      if (object === outer) {
        log.push('outerHeader');
        return outerHeader;
      }
      return null;
    },
    hasBody(object) { return false; },
    body(object) { return null; },
  }];
  globalThis.outer = outer;
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
  await Protocol.Runtime.evaluate({expression: setup});

  for (let i = 0; i < 2; ++i) {
    const paused = Protocol.Debugger.oncePaused();
    const evaluated =
        Protocol.Runtime.evaluate({expression: 'outer', generatePreview: true});
    await paused;
    const terminated = Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    InspectorTest.logMessage(await terminated);
    await evaluated;
  }

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'JSON.stringify(log)'});
  InspectorTest.log(result.value);

  InspectorTest.completeTest();
})();
