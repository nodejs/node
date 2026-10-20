// Flags: --expose-internals
'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { EventEmitter } = require('events');
const {
  waitForDebugger,
} = require('internal/debugger/inspect_helpers');

function assertListenersRemoved(client) {
  assert.strictEqual(
    client.listenerCount('NodeRuntime.waitingForDebugger'),
    0,
  );
  assert.strictEqual(client.listenerCount('close'), 0);
}

async function testWaitingNotification(beforeEnableReply) {
  const client = new EventEmitter();
  const calls = [];
  client.callMethod = common.mustCall(async (method) => {
    calls.push(method);
    const emitWaiting = () => {
      client.emit('NodeRuntime.waitingForDebugger');
    };
    if (method === 'NodeRuntime.enable') {
      if (beforeEnableReply) {
        emitWaiting();
      } else {
        setImmediate(emitWaiting);
      }
    } else {
      assert.strictEqual(method, 'NodeRuntime.disable');
    }
  }, 2);

  await waitForDebugger(client);
  assert.deepStrictEqual(calls, [
    'NodeRuntime.enable',
    'NodeRuntime.disable',
  ]);
  assertListenersRemoved(client);
}

async function testCloseWhileWaiting(beforeEnableReply) {
  const client = new EventEmitter();
  client.callMethod = common.mustCall((method) => {
    assert.strictEqual(method, 'NodeRuntime.enable');
    setImmediate(() => client.emit('close'));
    return beforeEnableReply ? new Promise(() => {}) : Promise.resolve();
  });

  await assert.rejects(
    waitForDebugger(client),
    {
      code: 'ERR_DEBUGGER_ERROR',
      message: 'Debugger session ended while waiting for target startup',
    },
  );
  assertListenersRemoved(client);
}

async function testCloseWhileDisabling() {
  const client = new EventEmitter();
  client.callMethod = common.mustCall((method) => {
    if (method === 'NodeRuntime.enable') {
      client.emit('NodeRuntime.waitingForDebugger');
      return Promise.resolve();
    }
    assert.strictEqual(method, 'NodeRuntime.disable');
    setImmediate(() => client.emit('close'));
    return new Promise(() => {});
  }, 2);

  await assert.rejects(
    waitForDebugger(client),
    {
      code: 'ERR_DEBUGGER_ERROR',
      message: 'Debugger session ended while waiting for target startup',
    },
  );
  assertListenersRemoved(client);
}

async function testEnableFailure() {
  const client = new EventEmitter();
  const expected = new Error('NodeRuntime.enable failed');
  client.callMethod = common.mustCall(async (method) => {
    assert.strictEqual(method, 'NodeRuntime.enable');
    throw expected;
  });

  await assert.rejects(
    waitForDebugger(client),
    (error) => {
      assert.strictEqual(error, expected);
      return true;
    },
  );
  assertListenersRemoved(client);
}

async function testDisableFailure() {
  const client = new EventEmitter();
  const expected = new Error('NodeRuntime.disable failed');
  client.callMethod = common.mustCall(async (method) => {
    if (method === 'NodeRuntime.enable') {
      client.emit('NodeRuntime.waitingForDebugger');
      return;
    }
    assert.strictEqual(method, 'NodeRuntime.disable');
    throw expected;
  }, 2);

  await assert.rejects(
    waitForDebugger(client),
    (error) => {
      assert.strictEqual(error, expected);
      return true;
    },
  );
  assertListenersRemoved(client);
}

(async () => {
  await testWaitingNotification(true);
  await testWaitingNotification(false);
  await testCloseWhileWaiting(true);
  await testCloseWhileWaiting(false);
  await testCloseWhileDisabling();
  await testEnableFailure();
  await testDisableFailure();
})().then(common.mustCall());
