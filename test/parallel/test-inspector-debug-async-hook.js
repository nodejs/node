'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const test = require('node:test');
const { NodeInstance } = require('../common/inspector-helper');

const script = `
import { createHook } from "async_hooks"
import fs from "fs"

const hook = createHook({
    after() {
    }
});
hook.enable(true);
console.log('Async hook enabled');
`;

test('inspector async hooks should not crash in debug build', async () => {
  const instance = new NodeInstance([
    '--inspect-brk=0',
  ], script);
  const session = await instance.connectInspectorSession();
  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send({ method: 'Runtime.enable' });
  await session.send({ method: 'Debugger.enable' });
  await session.send({ id: 6, method: 'Debugger.setAsyncCallStackDepth', params: { maxDepth: 32 } });
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.waitForDisconnect();
});
