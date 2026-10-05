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
  const response = await Protocol.Debugger.setScriptSource(
      {scriptId, scriptSource: 'function TestExpression(a, b) { return a * b; }'});
  InspectorTest.logMessage(response.error);
  InspectorTest.completeTest();
})();
