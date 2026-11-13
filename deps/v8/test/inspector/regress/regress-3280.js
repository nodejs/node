// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-3280.');

contextGroup.addScript(`
function* generator_local_2(a) {
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(message => {
    const scopes = message.params.callFrames[0].scopeChain.map(s => s.type);
    InspectorTest.log('scopes: ' + scopes.join(', '));
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'generator_local_2(1).next()'});
  InspectorTest.completeTest();
})();
