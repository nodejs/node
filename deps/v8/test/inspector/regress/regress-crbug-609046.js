// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-609046.');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(message => {
    const scopes = message.params.callFrames[0].scopeChain.map(s => s.type);
    InspectorTest.log('scopes: ' + scopes.join(', '));
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate(
      {expression: 'for (let a = 0; a < 3; a++) {\n  debugger;\n  eval();\n}'});
  InspectorTest.completeTest();
})();
