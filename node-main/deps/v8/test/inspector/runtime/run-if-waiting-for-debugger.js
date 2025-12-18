// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.runAsyncTestSuite([
  async function testTwoSessions() {
    InspectorTest.log('Tests Runtime.runIfWaitingForDebugger');

    const contextGroup = new InspectorTest.ContextGroup();
    const resumed = contextGroup.waitForDebugger().then(() => InspectorTest.log('execution resumed'));

    const session1 = contextGroup.connect();
    const session2 = contextGroup.connect();
    await session1.Protocol.Runtime.runIfWaitingForDebugger();
    InspectorTest.log('session 1 resumed');
    await session2.Protocol.Runtime.runIfWaitingForDebugger();
    InspectorTest.log('session 2 resumed');
    await resumed;
  },

  async function testSessionDisconnect() {
    InspectorTest.log('Tests Runtime.runIfWaitingForDebugger');

    const contextGroup = new InspectorTest.ContextGroup();
    const resumed = contextGroup.waitForDebugger().then(() => InspectorTest.log('execution resumed'));

    const session1 = contextGroup.connect();
    const session2 = contextGroup.connect();
    await session1.Protocol.Runtime.runIfWaitingForDebugger();
    InspectorTest.log('session 1 resumed');
    session2.disconnect();
    InspectorTest.log('session 2 disconnected');
    await resumed;
  }
]);
