// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that termination inside an error description getter stops the ' +
    'remaining preview work at call depth zero.');

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({
    expression: `
      var log = [];
      var pausing = false;
      var messageCalls = 0;
      globalThis.err = new Error();
      Object.setPrototypeOf(globalThis.err, {
        get name() {
          if (pausing) return 'Error';
          log.push('name');
          pausing = true;
          debugger;
          for (;;) {}
        },
        get message() {
          // Building Debugger.paused for callFrames[0].this formats err once
          // before terminateExecution() is sent; only log calls after that.
          if (++messageCalls > 1) log.push('message');
          return 'm';
        },
      });
      42;
    `
  });

  const paused = Protocol.Debugger.oncePaused();
  const evaluated =
      Protocol.Runtime.evaluate({expression: 'err', generatePreview: true});
  await paused;
  Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  const response = await evaluated;
  InspectorTest.log(
      'evaluate after terminateExecution: ' +
      (response.error ? response.error.message : 'NO ERROR'));

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'JSON.stringify(log)'});
  InspectorTest.log('getters that ran: ' + result.value);

  InspectorTest.completeTest();
})();
