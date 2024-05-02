// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Test that live editing the top-level function of an ES module does not work [crbug.com/1413447]');

const inputSnippet = `
//$1 import {b} from "./b.js";

export function foo() {
  console.log('foo');
}

export function bar() {
  //$2 console.log('bar');
}
`;

(async () => {
  await Protocol.Debugger.enable();

  contextGroup.addModule(inputSnippet, 'a.js', 3, 8);
  const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

  InspectorTest.log('Uncommenting the import line should fail:');
  const changedTopLevelModule = inputSnippet.replace('//$1 ', '');
  const response1 = await Protocol.Debugger.setScriptSource({ scriptId, scriptSource: changedTopLevelModule });
  InspectorTest.logMessage(response1.result);

  InspectorTest.log('Uncommenting the console.log line should work:');
  const changedFunction = inputSnippet.replace('//$2 ', '');
  const response2 = await Protocol.Debugger.setScriptSource({ scriptId, scriptSource: changedFunction });
  InspectorTest.logMessage(response2.result);

  InspectorTest.completeTest();
})();
