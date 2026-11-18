// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through various for loops.');

const source = `var s = 0;
var a = [1, 2, 3];
var b = [1, 2, 3, 4];
var c = [Promise.resolve(1), Promise.resolve(2), Promise.resolve(3)];
var null_value = null;
var i = 0;

async function f() {
  "use strict";
  debugger;                      // Break a
  var j;                         // Break b

  for (var i in null_value) {    // Break c
    s += a[i];
  }

  for (j in null_value) {        // Break d
    s += a[j];
  }

  for (var i in a) {             // Break e
    s += a[i];                   // Break E
  }

  for (j in a) {                 // Break f
    s += a[j];                   // Break F
  }

  for (let i in a) {             // Break g
    s += a[i];                   // Break G
  }

  for (var i of a) {             // Break h
    s += i;                      // Break H
  }

  for (j of a) {                 // Break i
    s += j;                      // Break I
  }

  for (let i  of  a) {           // Break j
    s += i;                      // Break J
  }

  for (var i = 0; i < 3; i++) {  // Break k
    s += a[i];                   // Break K
  }

  for (j = 0; j < 3; j++) {      // Break l
    s += a[j];                   // Break L
  }

  for (let i = 0; i < 3; i++) {  // Break m
    s += a[i];                   // Break M
  }

  for (let i of a) {}            // Break n

  [1, ...a]                      // Break o

  for await (let i of c) {       // Break p
    s += i;                      // Break P
  }

  for await (j of c) {           // Break q
    s += j;                      // Break Q
  }

}                                // Break y

function testComplete() {}

async function run() {
  await f();
  testComplete();                // Break z
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let log = [];
  const lines = source.split('\n');

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let loc = callFrames[0].location;
    let line = lines[loc.lineNumber];
    let col = loc.columnNumber;
    let lineWithBreakMarker =
        line.substring(0, col) + '#' + line.substring(col);
    InspectorTest.log(lineWithBreakMarker);
    let match = line.match(/\/\/ Break (\w)$/);
    if (match) {
      log.push(match[1] + col);
      if (match[1] === 'z') {
        await Protocol.Debugger.resume();
        return;
      }
    }
    await Protocol.Debugger.stepOver();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await Protocol.Runtime.evaluate({expression: 'run()', awaitPromise: true});

  let {result: {result: sResult}} =
      await Protocol.Runtime.evaluate({expression: 's'});

  InspectorTest.log('s: ' + sResult.value);
  InspectorTest.log('log: ' + JSON.stringify(log));

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
