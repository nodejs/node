// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that Debugger.setScriptSource doesn\'t crash with modules');

var module1 = `
export function foo() {
  return 42;
}`;

var editedModule1 = `
export function foo() {
  return 239;
}`;

var module2 = `
import { foo } from 'module1';
console.log(foo());
`;

var module1Id;
Protocol.Debugger.onScriptParsed(message => {
  if (message.params.url === 'module1')
    module1Id = message.params.scriptId;
});
Protocol.Debugger.enable()
  .then(() => contextGroup.addModule(module1, 'module1'))
  .then(() => contextGroup.addModule(module2, 'module2'))
  .then(() => InspectorTest.waitForPendingTasks())
  .then(() => Protocol.Debugger.setScriptSource({ scriptId: module1Id, scriptSource: editedModule1 }))
  .then(InspectorTest.logMessage)
  .then(InspectorTest.completeTest);
