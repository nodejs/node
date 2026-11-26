// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests exception events in generators.');

const source = `
function* g() {
  try {
    throw "a";  // Ordinary throw. Exception a
  } catch (e) {}
  try {
    yield 1;  // Caught internally. Exception b
  } catch (e) {}
  yield 2;
  yield 3;    // Caught externally. Exception c
  yield 4;
}

function run() {
  var g_obj = g();
  if (g_obj.next().value !== 1) throw new Error('fail 1');
  if (g_obj.throw("b").value !== 2) throw new Error('fail 2');
  if (g_obj.next().value !== 3) throw new Error('fail 3');
  try {
    g_obj.throw("c");
    throw new Error('should throw c');
  } catch (e) {
    if (e !== "c") throw e;
  }
  try {
    g_obj.throw("d");  // Closed generator. Exception d
    throw new Error('should throw d');
  } catch (e) {
    if (e !== "d") throw e;
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let log = [];
  let exception = null;

  Protocol.Debugger.onPaused(async ({params: {callFrames, data}}) => {
    try {
      let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
          {scriptId: callFrames[0].location.scriptId});
      let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
      let match = /Exception (\w)/.exec(line);
      if (!match) throw new Error('No match on line: ' + line);
      if (match[1] !== data.value) {
        throw new Error(`Mismatch: expected ${match[1]}, got ${data.value}`);
      }
      log.push(match[1]);
    } catch (e) {
      exception = e;
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'run()'});
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});

  InspectorTest.log(`log: ${JSON.stringify(log)}`);
  InspectorTest.log(`exception: ${exception}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
