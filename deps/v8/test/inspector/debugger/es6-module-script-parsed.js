// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Debugger.scriptParsed and Debugger.scriptFailedToParse with ES6 module');

let moduleSource = `
export function foo() {
  return 42;
}`;

contextGroup.addModule(moduleSource, 'module1.js');
contextGroup.addModule('}', 'module-with-syntax-error-1.js');

Protocol.Debugger.onScriptParsed(InspectorTest.logMessage);
Protocol.Debugger.onScriptFailedToParse(InspectorTest.logMessage);

InspectorTest.runTestSuite([
  function testLoadedModulesOnDebuggerEnable(next) {
    Protocol.Debugger.enable().then(next);
  },

  function testScriptEventsWhenDebuggerIsEnabled(next) {
    contextGroup.addModule(moduleSource, 'module2.js');
    contextGroup.addModule('}', 'module-with-syntax-error-2.js');
    InspectorTest.waitForPendingTasks().then(next);
  }
]);
