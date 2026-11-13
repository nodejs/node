// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Check that ScopeIterator can properly recreate the scope at every point ' +
    'when stepping through functions (part 5).');

const source = `
var q = 42;
var prefixes = [
  "debugger; ",
  "if (false) { try { throw 0; } catch(x) { this.x = x; } }; debugger; "
];
var with_bodies = [
  "with ({}) {}",
  "with ({x:1}) x",
  "with ({x:1}) x = 1",
  "with ({x:1}) x ",
  "with ({x:1}) x = 1 ",
  "with ({x:1}) x;",
  "with ({x:1}) x = 1;",
  "with ({x:1}) x; ",
  "with ({x:1}) x = 1; "
];

// Test global eval and function constructor.
for (var i = 0; i < prefixes.length; ++i) {
  var pre = prefixes[i];
  for (var j = 0; j < with_bodies.length; ++j) {
    var body = with_bodies[j];
    eval(pre + body);
    Function(pre + body)();
  }
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let pauseCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    pauseCount++;
    for (const scope of callFrames[0].scopeChain) {
      const response = await Protocol.Runtime.getProperties(
          {objectId: scope.object.objectId, ownProperties: true});
      if (response.error) {
        InspectorTest.log(
            `Error getting properties for scope ${scope.type}: ` +
            `${response.error.message}`);
      }
    }
    await Protocol.Debugger.stepInto();
  });

  await Protocol.Runtime.evaluate({expression: source});
  InspectorTest.log(`Total pauses: ${pauseCount}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
