// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests ScopeIterator scope recreation while stepping (part 2).');

const source = `
var q = 42;
var prefixes = [
  "debugger; ",
  "if (false) { try { throw 0; } catch(x) { this.x = x; } }; debugger; "
];
var bodies = [
  "1",
  "1 ",
  "1;",
  "1; ",
  "q",
  "q ",
  "q;",
  "q; ",
  "try { throw 'stuff' } catch (e) { e = 1; }",
  "try { throw 'stuff' } catch (e) { e = 1; } ",
  "try { throw 'stuff' } catch (e) { e = 1; };",
  "try { throw 'stuff' } catch (e) { e = 1; }; "
];

function test9() {
  debugger;
  for (var i = 0; i < prefixes.length; ++i) {
    var pre = prefixes[i];
    for (var j = 0; j < bodies.length; ++j) {
      var body = bodies[j];
      eval(pre + body);
      eval("'use strict'; " + pre + body);
    }
  }
}
test9();
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
