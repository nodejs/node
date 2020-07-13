// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test malformed sourceURL magic comment.');

(async function test() {
  await Protocol.Debugger.enable();
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "JSON.parse('//')",
    generatePreview: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "JSON.parse('//#')",
    generatePreview: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "JSON.parse('//# ')",
    generatePreview: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "JSON.parse('//# sourceURL')",
    generatePreview: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "JSON.parse('//# sourceURL=')",
    generatePreview: true,
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "JSON.parse('//# sourceURL=\"')",
    generatePreview: true,
  }));
  InspectorTest.completeTest();
})();
