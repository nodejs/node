// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-582048.');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    for (const frame of message.params.callFrames) {
      for (const scope of frame.scopeChain) {
        if (scope.type === 'global') continue;
        const {result: {result}} = await Protocol.Runtime.getProperties(
            {objectId: scope.object.objectId, ownProperties: true});
        const names = result.map(p => JSON.stringify(p.name));
        InspectorTest.log(`${scope.type}: [${names.join(', ')}]`);
      }
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate(
      {expression: '(function(a = 1) { debugger; })();'});
  InspectorTest.completeTest();
})();
