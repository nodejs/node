// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks basic ES6 modules support.');

var module1 = `
export function foo() {
  console.log('module1');
  return 42;
}
export let a1 = 1`;

var module2 = `
export function foo() {
  console.log('module2');
  return 239;
}
export let a2 = 2`;

var module3 = `
import { foo as foo1 } from 'module1';
import { foo as foo2 } from 'module2';
console.log(foo1());
console.log(foo2());
import { a1 } from 'module1';
import { a2 } from 'module2';
debugger;
`;

var module4 = '}';

session.setupScriptMap();
// We get scriptParsed events for modules ..
Protocol.Debugger.onScriptParsed(InspectorTest.logMessage);
// .. scriptFailed to parse for modules with syntax error ..
Protocol.Debugger.onScriptFailedToParse(InspectorTest.logMessage);
// .. API messages from modules contain correct stack trace ..
Protocol.Runtime.onConsoleAPICalled(message => {
  InspectorTest.log(`console.log(${message.params.args[0].value})`);
  session.logCallFrames(message.params.stackTrace.callFrames);
  InspectorTest.log('');
});
// .. we could break inside module and scope contains correct list of variables ..
Protocol.Debugger.onPaused(message => {
  InspectorTest.logMessage(message);
  Protocol.Runtime.getProperties({ objectId: message.params.callFrames[0].scopeChain[0].object.objectId})
    .then(InspectorTest.logMessage)
    .then(() => Protocol.Debugger.resume());
});
// .. we process uncaught errors from modules correctly.
Protocol.Runtime.onExceptionThrown(InspectorTest.logMessage);

Protocol.Runtime.enable();
Protocol.Debugger.enable()
  .then(() => contextGroup.addModule(module1, "module1"))
  .then(() => contextGroup.addModule(module2, "module2"))
  .then(() => contextGroup.addModule(module3, "module3"))
  .then(() => contextGroup.addModule(module4, "module4"))
  .then(() => InspectorTest.waitForPendingTasks())
  .then(InspectorTest.completeTest);
