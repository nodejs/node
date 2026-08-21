// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol, session} =
    InspectorTest.start('Regression test for crbug.com/1274529');

const url = 'foo.js';
contextGroup.addScript('function foo(){console.trace();}', 0, 0, url);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([async function test() {
  await Promise.all([Protocol.Runtime.enable(), Protocol.Debugger.enable()]);
  Protocol.Runtime.onConsoleAPICalled(
      ({params: {stackTrace: {callFrames}}}) => {
        session.logCallFrames(callFrames);
      });
  InspectorTest.logMessage('Calling foo() with dynamic name "foo"');
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  await Protocol.Runtime.evaluate(
      {expression: 'Object.defineProperty(foo, "name", {value: "bar"})'});
  InspectorTest.logMessage('Calling foo() with dynamic name "bar"');
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  await Promise.all([Protocol.Runtime.disable(), Protocol.Debugger.disable()]);
}]);
