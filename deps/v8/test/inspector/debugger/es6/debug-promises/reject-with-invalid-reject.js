// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests debug events with invalid reject closure in custom promise.');

const source = `
var log = [];

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

function MyPromise(resolver) {
  var reject = 1;
  var resolve = function() { };
  log.push("construct");
  resolver(resolve, reject);
};

MyPromise.prototype = new Promise(function() {});
p.constructor = MyPromise;

var q = p.then(
  function() {
    log.push("reject caught");
    return Promise.reject(new Error("caught"));
  });

log.push("end main");
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  Protocol.Debugger.onPaused(async ({params: {callFrames, data}}) => {
    let message = data && data.description ?
        data.description.split('\n')[0].replace(/^Error:\s*/, '') :
        '';
    InspectorTest.log(`Exception message: ${message}`);
    InspectorTest.log(`Call frame count: ${callFrames.length}`);
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: logResult}} =
      await Protocol.Runtime.evaluate({expression: 'log', returnByValue: true});
  InspectorTest.log(`Log: ${JSON.stringify(logResult.value)}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
