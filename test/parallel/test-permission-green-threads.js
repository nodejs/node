// Flags: --permission --allow-fs-read=* --allow-child-process --experimental-green-threads --no-warnings
'use strict';

// With the permission model, green threads need --allow-green-threads.

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const {
  GreenThread,
  setStallPolicy,
  setWarmPoolSize,
} = require('node:green_threads');

const denied = {
  code: 'ERR_ACCESS_DENIED',
  permission: 'GreenThreads',
  message: 'Access to this API has been restricted. Use ' +
    '--allow-green-threads to manage permissions.',
};

// Without --allow-green-threads, green threads can't be started, the warm
// pool can't be filled, and the stall policy can't be set. Nothing keeps the
// process alive afterwards.
{
  assert.strictEqual(process.permission.has('green-thread'), false);
  assert.throws(() => new GreenThread('1'), denied);
  assert.throws(() => setWarmPoolSize(1), denied);
  assert.throws(() => setStallPolicy({ timeoutMs: 100, onstall() {} }),
                denied);
  assert.throws(() => setStallPolicy(), denied);
}

function runChild(flags, script, stdout) {
  spawnSyncAndAssert(process.execPath, [
    ...flags, '--experimental-green-threads', '--no-warnings', '-e', script,
  ], { stdout, trim: true });
}

// With --allow-green-threads, they can, until the permission is dropped.
runChild(['--permission', '--allow-green-threads'], `
  const assert = require('assert');
  const {
    GreenThread,
    setStallPolicy,
    setWarmPoolSize,
  } = require('node:green_threads');
  (async () => {
    assert.strictEqual(process.permission.has('green-thread'), true);
    await setWarmPoolSize(1);
    await setWarmPoolSize(0);
    setStallPolicy({ timeoutMs: 1000, onstall() {} });
    setStallPolicy();
    const greenThread = new GreenThread('');
    assert.strictEqual(await greenThread.evaluate('1 + 1'), 2);
    await greenThread.terminate();
    process.permission.drop('green-thread');
    assert.strictEqual(process.permission.has('green-thread'), false);
    assert.throws(() => new GreenThread(''), { code: 'ERR_ACCESS_DENIED' });
    console.log('ok');
  })();
`, 'ok');

// In audit mode, they can be started, and the check is published.
runChild(['--permission-audit'], `
  const assert = require('assert');
  const dc = require('diagnostics_channel');
  const { GreenThread } = require('node:green_threads');
  const messages = [];
  dc.subscribe('node:permission-model:green-thread',
               ({ permission }) => messages.push(permission));
  (async () => {
    const greenThread = new GreenThread('');
    assert.strictEqual(await greenThread.evaluate('1 + 1'), 2);
    await greenThread.terminate();
    console.log(messages.join());
  })();
`, 'GreenThreads');
