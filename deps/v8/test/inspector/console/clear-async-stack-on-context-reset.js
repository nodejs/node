// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-async-stack-tagging-api

const {session, contextGroup, Protocol} = InspectorTest.start('Clear the async stack on navigations');

const script = `
function foo() {
  return console.scheduleAsyncTask("foo", true);
}

function bar(id) {
  console.startAsyncTask(id);
  console.trace(id); // When context is reset, this trace gets broken.
  // no finish
  console.cancelAsyncTask(id);
}

bar(foo());
`;

session.setupScriptMap();

(async () => {
  Protocol.Runtime.onConsoleAPICalled(({params}) => {
    InspectorTest.log(`---------- console.${params.type}: ${params.args[0].value} ----------`);
    session.logCallFrames(params.stackTrace.callFrames);
    session.logAsyncStackTrace(params.stackTrace.parent);
  });

  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });

  contextGroup.addScript(script, 0, 0, 'test.js');
  await Protocol.Debugger.onceScriptParsed();

  contextGroup.reset();

  // Run the same script again and make sure that the task stack has been reset.
  contextGroup.addScript(script, 0, 0, 'test.js');
  await Protocol.Debugger.onceScriptParsed();

  InspectorTest.completeTest();
})();
