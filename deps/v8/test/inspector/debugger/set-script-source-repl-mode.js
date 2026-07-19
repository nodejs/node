// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --inspector-live-edit --no-stress-incremental-marking

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

  // Run the GC to make sure the script's generator (for the top-level async
  // function, which is async since this is repl mode) is able to die.
  await Protocol.HeapProfiler.collectGarbage();

  const { result: { status } } = await Protocol.Debugger.setScriptSource({
    scriptId,
    scriptSource: changedScript,
  });
  InspectorTest.log(`Debugger.setScriptSource: ${status}`);

  InspectorTest.completeTest();
})();
