// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that termination inside a custom preview during ' +
    'Runtime.getProperties is not swallowed at call depth zero.');

const setup = `
  var log = [];
  const inner = {};
  const outer = {};
  const innerHeader = ['span', 'inner'];
  Object.defineProperty(innerHeader, 1, {
    get() {
      log.push('innerGetter');
      Promise.resolve().then(() => log.push('FAIL: microtask after termination'));
      debugger;
      for (;;) {}
    },
    enumerable: true,
    configurable: true,
  });
  const outerHeader = ['span', ['object', {object: inner}]];
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
  globalThis.holder = {p: outer};
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
  await Protocol.Runtime.evaluate({expression: setup});

  const {result: {result: holder}} =
      await Protocol.Runtime.evaluate({expression: 'holder'});

  const paused = Protocol.Debugger.oncePaused();
  const properties = Protocol.Runtime.getProperties(
      {objectId: holder.objectId, generatePreview: true});
  await paused;
  const terminated = Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  InspectorTest.logMessage(await terminated);
  await properties;

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'JSON.stringify(log)'});
  InspectorTest.log(result.value);

  InspectorTest.completeTest();
})();
