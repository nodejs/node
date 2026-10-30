// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-582051.');

contextGroup.addScript(`
var test_y = false;

function foo(a = 1) {
  var x = 2;
  debugger;
  eval("var y = 3");
  test_y = true;
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const scopes = message.params.callFrames[0].scopeChain;
    InspectorTest.log('scopes: ' + scopes.map(s => s.type).join(', '));
    for (let i = 0; i < 2; i++) {
      const {result: {result}} = await Protocol.Runtime.getProperties(
          {objectId: scopes[i].object.objectId, ownProperties: true});
      const props = result.map(p => `${p.name}: ${p.value.value}`);
      InspectorTest.log(
          `scope[${i}] (${scopes[i].type}): {${props.join(', ')}}`);
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'foo()'});
  InspectorTest.completeTest();
})();
