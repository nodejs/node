// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-private-methods

let {session, contextGroup, Protocol} = InspectorTest.start("Check static private methods in object preview.");

Protocol.Debugger.enable();
Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(dumpInternalProperties);

contextGroup.setupInjectedScriptEnvironment();

InspectorTest.runAsyncTestSuite([
  async function testPrivateMethods() {
    const expressions = [
        "class { static get #readOnly() { return 1; } }",
        "class { static set #writeOnly(val) { } }",
        "class { static #method() { return 1; } static get #accessor() { } static set #accessor(val) { } }",
        "class extends class { } { static #method() { return 1; } }",
        "class extends class { static #method() { return 1; } } { get #accessor() { } set #accessor(val) { } }",
    ];
    for (const expression of expressions) {
      InspectorTest.log(`expression: ${expression}`);
      // Currently the previews are strings of the source code of the classes
      await Protocol.Runtime.evaluate({
        expression: `console.table(${expression})`,
        generatePreview: true
      });
    }
  }
]);

function dumpInternalProperties(message) {
  try {
    InspectorTest.logMessage(message.params.args[0]);
  } catch {
    InspectorTest.logMessage(message);
  }
}
