// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into and out of generators.');

const source = `
function* g() {
  for (var i = 0; i < 3; ++i) {
    yield i;
  }
}

function run() {
  var i = g();
  debugger;
  for (var num of g()) {}
  i.next();

  var stop = "stop stepping";

  // Not stepped into.
  i.next();
  i.next();
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let yields = 0;
  let testScriptId;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let topFrame = callFrames[0];
    if (topFrame.location.scriptId !== testScriptId) {
      await Protocol.Debugger.resume();
      return;
    }
    let line = source.split('\n')[topFrame.location.lineNumber];
    InspectorTest.log(line.trim());
    if (/stop stepping/.test(line)) {
      await Protocol.Debugger.resume();
      return;
    }
    if (/yield/.test(line)) {
      yields++;
    }
    if (yields === 4) {
      await Protocol.Debugger.stepOut();
    } else {
      await Protocol.Debugger.stepInto();
    }
  });

  const scriptParsedPromise = Protocol.Debugger.onceScriptParsed();
  contextGroup.addScript(source, 0, 0, 'test.js');
  const {params: {scriptId}} = await scriptParsedPromise;
  testScriptId = scriptId;

  await Protocol.Runtime.evaluate({expression: 'run()'});
  InspectorTest.log('yields: ' + yields);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
