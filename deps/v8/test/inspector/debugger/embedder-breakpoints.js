// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests connecting a session with embedder-managed breakpoints.');

const source = `
function testFunction() {
  return 42;
}
`;

const url = 'test.js';
contextGroup.addScript(source, 0, 0, url);

InspectorTest.runAsyncTestSuite([
  async function testConnectWithEmbedderBreakpoints() {
    const embedderState = {
      urlBreakpoints: [
        {
          breakpointId: 'bp_from_embedder',
          lineNumber: 2,
          selectorType: 0, // kUrl
          selector: url,
          condition: '',
        }
      ]
    };

    InspectorTest.log('Connecting new session with embedder breakpoints...');
    const session2 = new InspectorTest.Session(contextGroup, true, embedderState);

    InspectorTest.log('Enabling runtime and debugger agents...');
    await session2.Protocol.Runtime.enable();
    const [scriptParsed, bpResolved] = await Promise.all([
      session2.Protocol.Debugger.onceScriptParsed(),
      session2.Protocol.Debugger.onceBreakpointResolved(),
      session2.Protocol.Debugger.enable(),
    ]);

    InspectorTest.log(`Script parsed: ${scriptParsed.params.url}`);
    InspectorTest.log(`Breakpoint resolved: ${bpResolved.params.breakpointId} at line ${bpResolved.params.location.lineNumber}`);

    InspectorTest.log('Evaluating testFunction()...');
    const evalPromise = session2.Protocol.Runtime.evaluate({expression: 'testFunction()'});
    const paused = await session2.Protocol.Debugger.oncePaused();
    InspectorTest.log(`Execution paused at line ${paused.params.callFrames[0].location.lineNumber}, hit breakpoints: ${JSON.stringify(paused.params.hitBreakpoints)}`);

    InspectorTest.log('Resuming...');
    await session2.Protocol.Debugger.resume();
    await evalPromise;

    InspectorTest.log('Disconnecting session...');
    session2.disconnect();
  },
]);
