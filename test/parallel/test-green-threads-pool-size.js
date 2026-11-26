// Flags: --expose-internals --no-warnings
'use strict';

// NODE_GREEN_THREADS_POOL_SIZE sets the number of pool threads.

require('../common');
const assert = require('assert');
const os = require('os');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const { internalBinding } = require('internal/test/binding');
  const {
    GreenThread,
    kEventMessage,
    kEventExit,
    kFlagTestingGlobals,
  } = internalBinding('green_thread');
  const handle = new GreenThread(
    'postMessage(__greenThreadInfo().poolSize); close();', 'test.js', 0, 0,
    kFlagTestingGlobals);
  handle.onevent = (type, id, ok, value) => {
    if (type === kEventMessage) console.log(value);
    if (type === kEventExit) handle.close();
  };
  return;
}

function poolSize(value) {
  const env = { ...process.env };
  if (value === undefined) {
    delete env.NODE_GREEN_THREADS_POOL_SIZE;
  } else {
    env.NODE_GREEN_THREADS_POOL_SIZE = value;
  }
  const child = spawnSync(process.execPath, [
    '--expose-internals', '--no-warnings', __filename, 'child',
  ], { env, encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
  return Number(child.stdout);
}

const defaultSize = Math.min(os.availableParallelism(), 8);
assert.strictEqual(poolSize(undefined), defaultSize);
assert.strictEqual(poolSize('1'), 1);
assert.strictEqual(poolSize('3'), 3);
assert.strictEqual(poolSize('12'), 12);
assert.strictEqual(poolSize('100000'), 1024);
for (const ignored of ['', '0', '-1', ' 2', '2x', 'abc']) {
  assert.strictEqual(poolSize(ignored), defaultSize, ignored);
}
