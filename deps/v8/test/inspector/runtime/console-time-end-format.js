// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks format of console.timeEnd output');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(message => {
  InspectorTest.log(message.params.args[0].value);
});

InspectorTest.runTestSuite([
  function zero(next) {
    checkInterval(0.0).then(next);
  },
  function verySmall(next) {
    checkInterval(1e-15).then(next);
  },
  function small(next) {
    checkInterval(0.001).then(next);
  },
  function regular(next) {
    checkInterval(1.2345).then(next);
  },
  function big(next) {
    checkInterval(10000.2345).then(next);
  },
  function veryBig(next) {
    checkInterval(1e+15 + 0.2345).then(next);
  },
  function huge(next) {
    checkInterval(1e+42).then(next);
  }
]);

function checkInterval(time) {
  utils.setCurrentTimeMSForTest(0.0);
  return Protocol.Runtime.evaluate({
    expression: `console.log('js: ' + ${time} + 'ms')`})
    .then(() => Protocol.Runtime.evaluate({
      expression: 'console.time(\'timeEnd\')'}))
    .then(() => utils.setCurrentTimeMSForTest(time))
    .then(() => Protocol.Runtime.evaluate({
      expression: 'console.timeEnd(\'timeEnd\')'}));
}
