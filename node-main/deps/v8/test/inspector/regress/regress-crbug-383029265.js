// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, Protocol, contextGroup } =
  InspectorTest.start('Don\'t crash when pausing in for-of loop header');

session.setupScriptMap();
contextGroup.addScript(`function pause() { debugger; }`);

const snippets = [`
  (function f() {
    for (const x of [2, pause()]) {
      function capture() { return x; }
    }
  })();
  `,`
  (function g() {
    for (const [x, p = pause()] of [[2]]) {
      function capture() { return x; }
    }
  })();
  `, `
  (async function h() {
    for await (const x of [Promise.resolve(2), Promise.resolve(pause())]) {
      function capture() { return x; }
    }
  })();
  `, `
  (async function i() {
    for await (const [x, p = pause()] of [Promise.resolve([2])]) {
      function capture() { return x; }
    }
  })();
  `
];

async function evaluateAndPrintScopes(snippet) {
  const pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({ expression: snippet, awaitPromise: true });

  const { params: { callFrames } } = await pausedPromise;

  // The top stack frame is `pause()`. We are only interested in the scope chain around the `for` loop.
  await session.logSourceLocation(callFrames[1].location);
  InspectorTest.logMessage(callFrames[1].scopeChain);

  await Protocol.Debugger.resume();
}

Protocol.Debugger.enable();

InspectorTest.runAsyncTestSuite([
  async function testSubjectPause() {
    await evaluateAndPrintScopes(snippets[0]);
  },
  async function testEachPause() {
    await evaluateAndPrintScopes(snippets[1]);
  },
  async function testAsyncSubjectPause() {
    await evaluateAndPrintScopes(snippets[2]);
  },
  async function testAsyncEachPause() {
    await evaluateAndPrintScopes(snippets[3]);
  },
]);
