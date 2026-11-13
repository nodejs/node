// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.start(
    'Untrusted sessions must not terminate execution via ' +
    'Debugger.resume({terminateOnResume:true})');

(async () => {
  const contextGroup = new InspectorTest.ContextGroup();
  const session = contextGroup.connect(/* isFullyTrusted */ false);
  const {Protocol} = session;

  await Protocol.Debugger.enable();

  InspectorTest.log('Runtime.terminateExecution on untrusted session:');
  InspectorTest.logMessage(await Protocol.Runtime.terminateExecution());

  const evaluated = Protocol.Runtime.evaluate(
      {expression: `globalThis.ran = false; debugger; globalThis.ran = true;`});
  await Protocol.Debugger.oncePaused();
  InspectorTest.log('Debugger.resume({terminateOnResume:true}):');
  InspectorTest.logMessage(
      await Protocol.Debugger.resume({terminateOnResume: true}));
  await Protocol.Debugger.resume();
  InspectorTest.logMessage(await evaluated);

  InspectorTest.completeTest();
})();
