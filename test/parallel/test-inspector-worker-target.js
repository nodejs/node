'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

common.skipIfInspectorDisabled();

const { NodeInstance } = require('../common/inspector-helper.js');

(async () => {
  const child = new NodeInstance(['--inspect-brk', '--experimental-worker-inspection'],
                                 '',
                                 fixtures.path('inspect-worker/index.js')
  );

  const session = await child.connectInspectorSession();
  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send({ method: 'Runtime.enable' });
  await session.send({ method: 'Debugger.enable' });
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.send({ method: 'NodeRuntime.disable' });
  await session.waitForNotification((notification) => {
    // // The main assertion here is that we do hit the loader script first.
    return notification.method === 'Debugger.scriptParsed' &&
            notification.params.url === 'node:internal/bootstrap/realm';
  });

  await session.waitForNotification('Debugger.paused');
  await session.send({ method: 'Debugger.resume' });

  const sessionId = '1';
  await session.waitForNotification('Target.targetCreated');
  await session.waitForNotification((notification) => {
    return notification.method === 'Target.attachedToTarget' &&
           notification.params.sessionId === sessionId;
  });

  await session.send({ method: 'Runtime.enable', sessionId });
  await session.send({ method: 'Debugger.enable', sessionId });
  await session.send({ method: 'Runtime.runIfWaitingForDebugger', sessionId });
  await session.send({ method: 'NodeRuntime.enable', sessionId });
  await session.waitForNotification((notification) => {
    return notification.method === 'Debugger.scriptParsed' &&
            notification.params.url === 'node:internal/bootstrap/realm';
  });
  await session.waitForNotification('Debugger.paused');
  await session.send({ method: 'Debugger.resume', sessionId });
  await session.waitForDisconnect();
})().then(common.mustCall());
