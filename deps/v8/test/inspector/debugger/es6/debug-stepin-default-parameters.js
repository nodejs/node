// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into default parameters.');

const source = `
function default_arg(x) {
  return "default";                 // d
}                                   // e

function f(arg0 = default_arg()) {  // f
  return arg0;                      // g
}                                   // h

function run() {
debugger;                           // a
var result = f();                   // b
done = true;                        // c
return result;
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let log = [];
  const max_breaks = 7;
  let testScriptId;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let topFrame = callFrames[0];
    let topSourceLine = source.split('\n')[topFrame.location.lineNumber];
    InspectorTest.log(topSourceLine);

    let entry = '';
    for (let frame of callFrames) {
      if (frame.location.scriptId !== testScriptId) continue;
      let line = source.split('\n')[frame.location.lineNumber];
      entry += line.substr(-1);
      entry += frame.location.columnNumber;
    }
    log.push(entry);
    InspectorTest.log('entry: ' + entry);

    if (log.length < max_breaks) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  const scriptParsedPromise = Protocol.Debugger.onceScriptParsed();
  contextGroup.addScript(source, 0, 0, 'test.js');
  const {params: {scriptId}} = await scriptParsedPromise;
  testScriptId = scriptId;

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'run()'});
  InspectorTest.log('result: ' + result.value);
  InspectorTest.log('log: ' + JSON.stringify(log));

  const expectedLog =
      ['a0', 'b13', 'f18b13', 'd2f18b13', 'd19f18b13', 'g14b13', 'c0'];
  InspectorTest.log(
      'logMatches: ' + (JSON.stringify(log) === JSON.stringify(expectedLog)));

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
