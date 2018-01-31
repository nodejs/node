// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks format of console.timeEnd output');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(message => {
  InspectorTest.log(message.params.args[0].value);
});

InspectorTest.runAsyncTestSuite([
  function zero() {
    return checkInterval(0.0);
  },
  function verySmall() {
    return checkInterval(1e-15);
  },
  function small() {
    return checkInterval(0.001);
  },
  function regular() {
    return checkInterval(1.2345);
  },
  function big() {
    return checkInterval(10000.2345);
  },
  function veryBig() {
    return checkInterval(1e+15 + 0.2345);
  },
  function huge() {
    return checkInterval(1e+42);
  },
  function undefinedAsLabel() {
    return checkInterval(1.0, 'undefined');
  },
  function emptyAsLabel() {
    return checkInterval(1.0, '');
  }
]);

async function checkInterval(time, label) {
  label = label === undefined ? '\'timeEnd\'' : label;
  utils.setCurrentTimeMSForTest(0.0);
  Protocol.Runtime.evaluate({
    expression: `console.log('js: ' + ${time} + 'ms')`
  });
  await Protocol.Runtime.evaluate({expression: `console.time(${label})`});
  utils.setCurrentTimeMSForTest(time);
  await Protocol.Runtime.evaluate({expression: `console.timeEnd(${label})`});
}
