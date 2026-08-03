// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check private methods in object preview.");

Protocol.Debugger.enable();
Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(dumpInternalProperties);

contextGroup.setupInjectedScriptEnvironment();

InspectorTest.runAsyncTestSuite([
  async function testPrivateMethods() {
    const expressions = [
        "new class extends class { constructor() { return new Proxy({}, {}); } } { #method() { return 1; } }",
        "new class { #method() { return 1; } get #accessor() { } set #accessor(val) { } }",
        "new class extends class { #method() { return 1; } } { get #accessor() { } set #accessor(val) { } }",
    ];
    for (const expression of expressions) {
      InspectorTest.log(`expression: ${expression}`);
      await Protocol.Runtime.evaluate({
        expression: `console.table(${expression})`,
        generatePreview: true
      });
    }
  }
]);

function dumpInternalProperties(message) {
  try {
    InspectorTest.logMessage(message.params.args[0].preview.properties);
  } catch {
    InspectorTest.logMessage(message);
  }
}
