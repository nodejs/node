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
  }
]);

function setupSessions(num) {
  const contextGroup = new InspectorTest.ContextGroup();
  const sessions = [];
  for (let i = 0; i < num; ++i) {
    const session = contextGroup.connect();
    sessions.push(session);
    session.Protocol.Runtime.enable();
    session.Protocol.Runtime.onBindingCalled(msg => {
      InspectorTest.log(`binding called in session${i + 1}`);
      InspectorTest.logMessage(msg);
    });
  }
  return {contextGroup, sessions};
}
