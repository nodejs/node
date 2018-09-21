// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.setScriptSource');

contextGroup.addScript(
`function TestExpression(a, b) {
    return a + b;
}`);

(async function test() {
  Protocol.Debugger.enable();
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();
  const {result: {result: {value}}} =
      await Protocol.Runtime.evaluate({expression: 'TestExpression(2, 4)'});
  InspectorTest.log(`TestExpression(2,4) === ${value}`);
  {
    const {result: {scriptSource}} =
        await Protocol.Debugger.getScriptSource({scriptId});
    InspectorTest.log(`Update current script source 'a + b' -> 'a * b'..`);
    const {result} = await Protocol.Debugger.setScriptSource(
        {scriptId, scriptSource: scriptSource.replace('a + b', 'a * b')})
    InspectorTest.logMessage(result);
    const {result: {result: {value}}} =
        await Protocol.Runtime.evaluate({expression: 'TestExpression(2, 4)'});
    InspectorTest.log(`TestExpression(2,4) === ${value}`);
  }
  {
    const {result: {scriptSource}} =
        await Protocol.Debugger.getScriptSource({scriptId});
    InspectorTest.log(`Update current script source 'a * b' -> 'a # b'..`);
    const {result} = await Protocol.Debugger.setScriptSource(
        {scriptId, scriptSource: scriptSource.replace('a * b', 'a # b')})
    InspectorTest.logMessage(result);
    const {result: {result: {value}}} =
        await Protocol.Runtime.evaluate({expression: 'TestExpression(2, 4)'});
    InspectorTest.log(`TestExpression(2,4) === ${value}`);
  }
  InspectorTest.completeTest();
})();
