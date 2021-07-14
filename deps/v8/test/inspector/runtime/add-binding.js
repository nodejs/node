// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Test for Runtime.addBinding.');

InspectorTest.runAsyncTestSuite([
  async function testBasic() {
    const {contextGroup, sessions: [session1, session2]} = setupSessions(2);

    InspectorTest.log('\nAdd binding inside session1..');
    session1.Protocol.Runtime.addBinding({name: 'send'});
    InspectorTest.log('Call binding..');
    await session1.Protocol.Runtime.evaluate({expression: `send('payload')`});

    InspectorTest.log('\nAdd binding inside session2..');
    session2.Protocol.Runtime.addBinding({name: 'send'});
    InspectorTest.log('Call binding..');
    await session2.Protocol.Runtime.evaluate({expression: `send('payload')`});

    InspectorTest.log('\nDisable agent inside session1..');
    session1.Protocol.Runtime.disable();
    InspectorTest.log('Call binding..');
    await session2.Protocol.Runtime.evaluate({expression: `send('payload')`});

    InspectorTest.log('\nDisable agent inside session2..');
    session2.Protocol.Runtime.disable();
    InspectorTest.log('Call binding..');
    await session2.Protocol.Runtime.evaluate({expression: `send('payload')`});

    InspectorTest.log('\nEnable agent inside session1..');
    session1.Protocol.Runtime.enable();
    InspectorTest.log('Call binding..');
    await session2.Protocol.Runtime.evaluate({expression: `send('payload')`});
  },

  async function testReconnect() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    InspectorTest.log('\nAdd binding inside session..');
    await session.Protocol.Runtime.addBinding({name: 'send'});
    InspectorTest.log('Reconnect..');
    session.reconnect();
    await session.Protocol.Runtime.evaluate({expression: `send('payload')`});
  },

  async function testBindingOverrides() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    InspectorTest.log('\nAdd send function on global object..');
    session.Protocol.Runtime.evaluate({expression: 'send = () => 42'});
    InspectorTest.log('Add binding inside session..');
    session.Protocol.Runtime.addBinding({name: 'send'});
    InspectorTest.log('Call binding..');
    await session.Protocol.Runtime.evaluate({expression: `send('payload')`});
  },

  async function testRemoveBinding() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    InspectorTest.log('\nAdd binding inside session..');
    session.Protocol.Runtime.addBinding({name: 'send'});
    InspectorTest.log('Call binding..');
    await session.Protocol.Runtime.evaluate({expression: `send('payload')`});
    InspectorTest.log('Remove binding inside session..');
    session.Protocol.Runtime.removeBinding({name: 'send'});
    InspectorTest.log('Call binding..');
    await session.Protocol.Runtime.evaluate({expression: `send('payload')`});
  },

  async function testAddBindingToContextById() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    const contextId1 = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    contextGroup.createContext();
    const contextId2 = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextId: contextId2});
    const expression = `frobnicate('message')`;

    InspectorTest.log('Call binding in default context (binding should NOT be exposed)');
    await session.Protocol.Runtime.evaluate({expression});

    InspectorTest.log('Call binding in target context (binding should be exposed)');
    await session.Protocol.Runtime.evaluate({expression, contextId: contextId2});

    InspectorTest.log('Call binding in newly created context (binding should NOT be exposed)');
    contextGroup.createContext();
    const contextId3 = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;
    await session.Protocol.Runtime.evaluate({expression, contextId: contextId3});
  },

  async function testAddBindingToMultipleContextsById() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    const contextId1 = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    contextGroup.createContext();
    const contextId2 = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextId: contextId1});
    await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextId: contextId2});
    const expression = `frobnicate('message')`;

    InspectorTest.log('Call binding in default context (binding should be exposed)');
    await session.Protocol.Runtime.evaluate({expression});

    InspectorTest.log('Call binding in target context (binding should be exposed)');
    await session.Protocol.Runtime.evaluate({expression, contextId: contextId2});
  },

  async function testAddBindingToMultipleContextsInDifferentContextGroups() {
    const sessions1 = setupSessions(1, 'group1/');
    const session1 = sessions1.sessions[0];
    const contextId1 = (await session1.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    const sessions2 = setupSessions(1, 'group2/');
    const session2 = sessions2.sessions[0];
    const contextId2 = (await session2.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    await session1.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextId: contextId1});
    await session2.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextId: contextId2});
    const expression = `frobnicate('message')`;

    InspectorTest.log('Call binding in default context (binding should be exposed)');
    await session1.Protocol.Runtime.evaluate({expression, contextId: contextId1});

    InspectorTest.log('Call binding in target context (binding should be exposed)');
    await session2.Protocol.Runtime.evaluate({expression, contextId: contextId2});
  },

  async function testAddBindingToContextByName() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    const defaultContext = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    contextGroup.createContext("foo");
    const contextFoo = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    contextGroup.createContext("bar");
    const contextBar = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextName: 'foo'});
    const expression = `frobnicate('message')`;

    InspectorTest.log('Call binding in default context (binding should NOT be exposed)');
    await session.Protocol.Runtime.evaluate({expression});

    InspectorTest.log('Call binding in Foo (binding should be exposed)');
    await session.Protocol.Runtime.evaluate({expression, contextId: contextFoo});

    InspectorTest.log('Call binding in Bar (binding should NOT be exposed)');
    await session.Protocol.Runtime.evaluate({expression, contextId: contextBar});

    contextGroup.createContext("foo");
    const contextFoo2 = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    InspectorTest.log('Call binding in newly-created Foo (binding should be exposed)');
    await session.Protocol.Runtime.evaluate({expression, contextId: contextFoo2});

    contextGroup.createContext("bazz");
    const contextBazz = (await session.Protocol.Runtime.onceExecutionContextCreated()).params.context.id;

    InspectorTest.log('Call binding in newly-created Bazz (binding should NOT be exposed)');
    await session.Protocol.Runtime.evaluate({expression, contextId: contextBazz});
  },

  async function testErrors() {
    const {contextGroup, sessions: [session]} = setupSessions(1);
    let err = await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextName: ''});
    InspectorTest.logMessage(err);
    err = await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextName: 'foo', executionContextId: 1});
    InspectorTest.logMessage(err);
    err = await session.Protocol.Runtime.addBinding({name: 'frobnicate', executionContextId: 2128506});
    InspectorTest.logMessage(err);
  }

]);

function setupSessions(num, prefix = '') {
  const contextGroup = new InspectorTest.ContextGroup();
  const sessions = [];
  for (let i = 0; i < num; ++i) {
    const session = contextGroup.connect();
    sessions.push(session);
    session.Protocol.Runtime.enable();
    session.Protocol.Runtime.onBindingCalled(msg => {
      InspectorTest.log(`binding called in ${prefix}session${i + 1}`);
      InspectorTest.logMessage(msg);
    });
  }
  return {contextGroup, sessions};
}
