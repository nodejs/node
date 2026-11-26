// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into accessor ICs.');

const source = `
function get() {
  return 3;                       // Break
}                                 // Break

function set(x) {
  this.x = x;                     // Break
}                                 // Break

var o = {};
Object.defineProperty(o, "get", { get : get });
Object.defineProperty(o, "set", { set : set });

function f() {
  for (var i = 0; i < 10; i++) {  // Break
    o.get;                        // Break
    o.set = 1;                    // Break
  }
}                                 // Break

debugger;                         // Break
f();                              // Break
let done = true;                  // Break
`;

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  let allLinesHaveBreakComment = true;

  Protocol.Debugger.onPaused(async message => {
    let callFrames = message.params.callFrames;
    let loc = callFrames[0].location;
    let line = source.split('\n')[loc.lineNumber];
    if (line && line.includes('// Break')) {
      breakCount++;
      if (breakCount < 86) {
        Protocol.Debugger.stepInto();
      } else {
        Protocol.Debugger.resume();
      }
    } else {
      if (line && line.trim().length > 0) {
        allLinesHaveBreakComment = false;
        InspectorTest.log('Unexpected break on: ' + line);
      }
      Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await InspectorTest.waitForPendingTasks();
  InspectorTest.log('breakCount: ' + breakCount);
  InspectorTest.log('allLinesHaveBreakComment: ' + allLinesHaveBreakComment);
  InspectorTest.completeTest();
})();
