// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { Protocol, contextGroup } = InspectorTest.start('Don\'t crash when creating a new var in a module via Debugger.evaluateOnCallFrame');

(async () => {
  await Protocol.Debugger.enable();

  contextGroup.addModule(`
    (function foo() { eval('var test2 = 42;'); debugger; })();
  `, 'module1');

  const { params: { callFrames: [{ callFrameId }] }} = await Protocol.Debugger.oncePaused();
  const { result: { result }} = await Protocol.Debugger.evaluateOnCallFrame({
    expression: 'var test = 1;',
    callFrameId,
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
