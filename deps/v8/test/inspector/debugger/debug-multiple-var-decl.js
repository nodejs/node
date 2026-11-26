// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping through multiple var/let/const declarations.');

const scriptSource = `function f() {
  var l1 = 1,    // l
      l2,        // m
      l3 = 3;    // n
  let l4,        // o
      l5 = 5,    // p
      l6 = 6;    // q
  const l7 = 7,  // r
        l8 = 8,  // s
        l9 = 9;  // t
  return 0       // u
  ;              // v
}

debugger;        // a
var g1 = 1,      // b
    g2 = 2,      // c
    g3;          // d
let g4 = 4,      // e
    g5,          // f
    g6 = 6;      // g
const g7 = 7,    // h
      g8 = 8,    // i
      g9 = f();  // j
`;

const scriptLines = scriptSource.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  const log = [];
  let doneResolve;
  const donePromise = new Promise(resolve => {
    doneResolve = resolve;
  });

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const loc = callFrames[0].location;
    const line = scriptLines[loc.lineNumber];
    const match = line.match(/\/\/ (\w)$/);
    log.push(match[1] + loc.columnNumber);
    if (match[1] !== 'v') {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
      doneResolve();
    }
  });

  contextGroup.addScript(scriptSource, 0, 0, 'test.js');
  await donePromise;

  InspectorTest.log('log: ' + JSON.stringify(log));
  InspectorTest.completeTest();
})();
