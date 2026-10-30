// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-break-default-constructor.');

contextGroup.addScript(
    `"use strict";
class Base {
  constructor() {
    var x = 1;     // 2.
  }                // 3.
}

class Derived extends Base {}  // 1. // 4.
`,
    0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 7, columnNumber: 0, url: 'test.js'});

  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    Protocol.Debugger.stepInto();
  });

  await Protocol.Runtime.evaluate({expression: 'new Derived()'});
  InspectorTest.completeTest();
})();
