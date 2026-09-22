// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that resetting the context group during wrapObject does not UAF.');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  const secondaryContextGroup = new InspectorTest.ContextGroup();
  const secondarySession = secondaryContextGroup.connect();

  await secondarySession.Protocol.Debugger.enable();
  await secondarySession.Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  secondarySession.Protocol.Debugger.onPaused(message => {
    InspectorTest.log(
        'Secondary session paused. Exception captured successfully.');
    // Resume and finish test.
    secondarySession.Protocol.Debugger.resume();
    InspectorTest.completeTest();
  });

  InspectorTest.log('Evaluating script in secondary context...');

  secondarySession.Protocol.Runtime.evaluate({
    expression: `
      Error.prepareStackTrace = function(error, stack) {
        // Destroy the context group while we are inside wrapObject.
        utils.resetContextGroup(${secondaryContextGroup.id});
        return stack;
      };
      throw new Error("Trigger UAF");
    `
  });
})();
