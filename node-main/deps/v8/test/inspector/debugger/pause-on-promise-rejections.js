// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test Debugger.paused reason for promise rejections');

(async function test() {
  Protocol.Debugger.enable();
  Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  InspectorTest.log('Check Promise.reject in script:');
  contextGroup.addScript(`Promise.reject(new Error())`);
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.log('Check Promise.reject in Runtime.evaluate:');
  Protocol.Runtime.evaluate({expression: `Promise.reject(new Error())`});
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.log('Check Promise.reject in async function:');
  Protocol.Runtime.evaluate(
      {expression: `(async function() { await Promise.reject(); })()`});
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.log('Check throw in async function:');
  Protocol.Runtime.evaluate({
    expression: `(async function() { await Promise.resolve(); throw 42; })()`
  });
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.log('Check reject from constructor:');
  Protocol.Runtime.evaluate({
    expression: 'new Promise((_, reject) => reject(new Error())).catch(e => {})'
  });
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.log('Check reject from thenable job:');
  Protocol.Runtime.evaluate({
    expression:
        `Promise.resolve().then(() => Promise.reject(new Error())).catch(e => 0)`
  });
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.log(
      'Check caught exception in async function (should be exception):');
  Protocol.Runtime.evaluate({
    expression: `(async function() {
      await Promise.resolve();
      try {
        throw 42;
      } catch (e) {}
    })()`
  });
  await logPausedReason();
  await Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();

async function logPausedReason() {
  const {params: {reason}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log(reason + '\n');
}
