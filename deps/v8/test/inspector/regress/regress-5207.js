// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noanalyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-5207.');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const scopes = message.params.callFrames[0].scopeChain;
    for (let i = 0; i < scopes.length; i++) {
      const {result: {result}} = await Protocol.Runtime.getProperties(
          {objectId: scopes[i].object.objectId, ownProperties: true});
      const propA = result.find(p => p.name === 'a');
      InspectorTest.log(`scope[${i}] (${scopes[i].type}): a=${
          propA ? propA.value.value : undefined}`);
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate(
      {expression: `'use strict';\neval("var a = 1; debugger;");`});
  InspectorTest.completeTest();
})();
