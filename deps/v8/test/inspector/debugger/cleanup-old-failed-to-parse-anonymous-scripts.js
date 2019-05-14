// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that inspector collects old faied to parse anonymous scripts.');

(async function main() {
  Protocol.Debugger.enable();
  const scriptIds = [];
  Protocol.Debugger.onScriptFailedToParse(
      message => scriptIds.push(message.params.scriptId));
  InspectorTest.log('Generate 1000 scriptFailedToParse events');
  await Protocol.Runtime.evaluate({
    expression: `for (var i = 0; i < 1000; ++i) {
      try { JSON.parse('}'); } catch(e) {}
    }`
  });
  await dumpScriptIdsStats(scriptIds);
  InspectorTest.log(
      'Generate three scriptFailedToParse event for non anonymous script');
  for (var i = 0; i < 3; ++i) {
    await Protocol.Runtime.evaluate({expression: '}//# sourceURL=foo.js'});
  }
  await dumpScriptIdsStats(scriptIds);
  InspectorTest.log(
      'Generate one more scriptFailedToParse event for anonymous script');
  await Protocol.Runtime.evaluate(
      {expression: `try {JSON.parse('}');} catch(e){}`});
  await dumpScriptIdsStats(scriptIds);
  InspectorTest.log('Check that latest script is still available');
  InspectorTest.logMessage(await Protocol.Debugger.getScriptSource(
      {scriptId: scriptIds[scriptIds.length - 1]}));
  InspectorTest.completeTest();
})();

async function dumpScriptIdsStats(scriptIds) {
  let errors = 0;
  let success = 0;
  for (let scriptId of scriptIds) {
    const result =
        await Protocol.Debugger.getScriptSource({scriptId: scriptId});
    if (result.error)
      ++errors;
    else
      ++success;
  }
  InspectorTest.log(`error:${errors}\nsuccess:${success}`);
}
