// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug/1283049');

session.setupScriptMap();

contextGroup.addInlineScript(
    `function foo() { debugger; }
//# sourceURL=foo.js`,
    'regress-crbug-1283049.js');

InspectorTest.runAsyncTestSuite([async function test() {
  await Promise.all([
    Protocol.Runtime.enable(),
    Protocol.Debugger.enable(),
  ]);
  const evalPromise = Protocol.Runtime.evaluate({expression: 'foo()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  session.logCallFrames(callFrames);
  await Promise.all([
    Protocol.Debugger.resume(),
    evalPromise,
    Protocol.Runtime.disable(),
    Protocol.Debugger.disable(),
  ]);
}]);
