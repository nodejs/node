// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into Proxy traps.');

const source = `var target = {};
var handler = {
  has: function(target, name) {
    return true;                     // h
  },                                 // i
  get: function(target, name) {
    return 42;                       // j
  },                                 // k
  set: function(target, name, value) {
    return false;                    // l
  },                                 // m
}

var proxy = new Proxy(target, handler);

function testComplete() {}

debugger;                            // a
var has = "step" in proxy;           // b
var get = proxy.step;                // c
proxy.step = 43;                     // d

testComplete();                      // g
`;

(async function test() {
  await Protocol.Debugger.enable();

  let log = [];
  const lines = source.split('\n');

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    var entry = '';
    for (var i = 0; i < callFrames.length; i++) {
      let line = lines[callFrames[i].location.lineNumber];
      entry += line.substr(-1);
      entry += callFrames[i].location.columnNumber;
    }
    log.push(entry);
    InspectorTest.log(entry);
    if (entry !== 'g0') {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await InspectorTest.waitForPendingTasks();

  let {result: {result: hasResult}} =
      await Protocol.Runtime.evaluate({expression: 'has'});
  let {result: {result: getResult}} =
      await Protocol.Runtime.evaluate({expression: 'get'});

  InspectorTest.log('has: ' + hasResult.value);
  InspectorTest.log('get: ' + getResult.value);
  InspectorTest.log('log: ' + JSON.stringify(log));

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
