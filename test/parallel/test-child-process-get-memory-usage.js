'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { fork } = require('child_process');
const { once } = require('events');

const childScript = fixtures.path('child-process-get-memory-usage-child.js');

async function spawnChild(stdio = ['ignore', 'ignore', 'ignore', 'ipc']) {
  const child = fork(childScript, [], { stdio });
  await once(child, 'message'); // wait for "ready"
  return child;
}

async function testSuccessfulMemoryUsageRetrieval() {
  const child = await spawnChild();
  const usage = await child.getMemoryUsage();

  assert.strictEqual(typeof usage, 'object');
  for (const key of ['rss', 'heapTotal', 'heapUsed', 'external', 'arrayBuffers']) {
    assert.strictEqual(typeof usage[key], 'number');
  }

  child.send('exit');
  await once(child, 'exit');
}

async function testRejectsWhenNotRunning() {
  const child = await spawnChild();
  child.kill();
  await once(child, 'exit');

  await assert.rejects(child.getMemoryUsage(), {
    code: 'ERR_CHILD_PROCESS_NOT_RUNNING',
  });
}

async function testRejectsWhenChannelClosed() {
  const child = await spawnChild();
  child.disconnect();

  await assert.rejects(child.getMemoryUsage(), {
    code: 'ERR_IPC_CHANNEL_CLOSED',
  });

  child.kill();
  await once(child, 'exit');
}

async function main() {
  await testSuccessfulMemoryUsageRetrieval();
  await testRejectsWhenNotRunning();
  await testRejectsWhenChannelClosed();
}

main().then(common.mustCall());
