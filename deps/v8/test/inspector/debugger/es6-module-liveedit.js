// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Checks liveedit with ES6 modules.');

const moduleSource = `
export function foo() {
  console.log('module1');
  return 42;
}
foo()`;

const newModuleSource = `
export function foo() {
  console.log('patched module1');
  return 42;
}
foo()`;

const callFooSource = `
import { foo } from 'module';
foo();`;

(async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  contextGroup.addModule(moduleSource, 'module');
  const [{ params: { scriptId } }, { params: { args }}] = [
    await Protocol.Debugger.onceScriptParsed(),
    await Protocol.Runtime.onceConsoleAPICalled()
  ];
  InspectorTest.log('console.log message from function before patching:')
  InspectorTest.logMessage(args[0]);

  const {result} = await Protocol.Debugger.setScriptSource({
    scriptId,
    scriptSource: newModuleSource
  });
  InspectorTest.log('Debugger.setScriptSource result:');
  InspectorTest.logMessage(result);

  contextGroup.addModule(callFooSource, 'callFoo');
  const { params: {args: patchedArgs } } =
      await Protocol.Runtime.onceConsoleAPICalled();
  InspectorTest.log('console.log message from function after patching:')
  InspectorTest.logMessage(patchedArgs[0]);
  InspectorTest.completeTest();
})()
