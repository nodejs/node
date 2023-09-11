// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Test that sloppy function block hoisting works with debug-evaluate.');

(async () => {
  await Protocol.Debugger.enable();

  Protocol.Runtime.evaluate({
    expression: `debugger`,
    replMode: true,
  });

  const {params: { callFrames: [{callFrameId}]}} = await Protocol.Debugger.oncePaused();

  InspectorTest.log('Paused on debugger statement');

  const { result } = await Protocol.Debugger.evaluateOnCallFrame({
    expression: '{ function f() {f} }; f();',
    callFrameId,
  });
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
