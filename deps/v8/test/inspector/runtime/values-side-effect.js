// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that values() properly propagates side-effect and normal exceptions.');

async function test() {
  await Protocol.Runtime.evaluate({
    expression:
        `globalThis.obj2 = { get a() { throw new Error('boom'); }, b: 2 };`,
  });

  InspectorTest.log('Test 1: normal exception');
  let result = await Protocol.Runtime.evaluate({
    expression: `try { values(obj2); } catch (e) { 'caught: ' + e.message }`,
    includeCommandLineAPI: true,
  });
  InspectorTest.logMessage(result.result);

  await Protocol.Runtime.evaluate({
    expression:
        `globalThis.obj = { get a() { globalThis.x = 1; }, get b() { return 2; } };`,
  });

  InspectorTest.log('Test 2: throwOnSideEffect');
  result = await Protocol.Runtime.evaluate({
    expression: `values(obj)`,
    includeCommandLineAPI: true,
    throwOnSideEffect: true,
  });
  InspectorTest.logMessage(result.result);
  result = await Protocol.Runtime.evaluate({
    expression: `globalThis.x`,
  });
  InspectorTest.logMessage(result.result);

  InspectorTest.log('Test 3: plain object sanity check');
  result = await Protocol.Runtime.evaluate({
    expression: `values({ a: 1, b: 2 })`,
    includeCommandLineAPI: true,
  });
  InspectorTest.logMessage(result.result);

  InspectorTest.completeTest();
}

test();
