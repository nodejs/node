// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-4703.');

contextGroup.addScript(`
function run(arg, ...rest) {
  var one = 1;
  function inner() {
    one;
    arg;
  }
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(message => {
    const scopes = message.params.callFrames[0].scopeChain;
    InspectorTest.log(`Scope types: ${scopes.map(s => s.type).join(', ')}`);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'run()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
