// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
  InspectorTest.start('Check that setScriptSource works with REPL mode');

const script = `
  function foo() { return 42; }
  const bar = foo();
`;
const changedScript = script.replace('const', 'let');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Runtime.evaluate({
    expression: script,
    replMode: true,
  });
  const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

  const { result: { status } } = await Protocol.Debugger.setScriptSource({
    scriptId,
    scriptSource: changedScript,
  });
  InspectorTest.log(`Debugger.setScriptSource: ${status}`);

  InspectorTest.completeTest();
})();
