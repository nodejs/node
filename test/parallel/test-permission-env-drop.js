'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const { spawnSyncAndAssert } = require('../common/child_process');

const env = {
  ...process.env,
  PERMISSION_ENV_ONE: 'one',
  PERMISSION_ENV_TWO: 'two',
  TZ: 'UTC',
};

// Dropping a variable removes it from the environment.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-env=PERMISSION_ENV_ONE,PERMISSION_ENV_TWO', '-e',
    `
    const assert = require('assert');
    const dc = require('diagnostics_channel');
    const drops = [];
    dc.subscribe('node:permission-model:env', (message) => {
      if (message.drop) drops.push(message.resource);
    });

    assert.strictEqual(process.permission.has('env', 'PERMISSION_ENV_ONE'), true);
    process.permission.drop('env', 'PERMISSION_ENV_ONE');
    assert.strictEqual(process.permission.has('env', 'PERMISSION_ENV_ONE'), false);
    assert.strictEqual(process.env.PERMISSION_ENV_ONE, undefined);
    assert.strictEqual(process.permission.has('env', 'PERMISSION_ENV_TWO'), true);
    assert.strictEqual(process.env.PERMISSION_ENV_TWO, 'two');

    // Dropping the whole scope removes everything except the variables
    // Node.js reads itself.
    process.permission.drop('env');
    assert.strictEqual(process.permission.has('env', 'PERMISSION_ENV_TWO'), false);
    assert.strictEqual(process.env.PERMISSION_ENV_TWO, undefined);
    assert.strictEqual(process.permission.has('env', 'TZ'), true);
    assert.strictEqual(process.env.TZ, 'UTC');

    assert.deepStrictEqual(drops, ['PERMISSION_ENV_ONE', '']);
    `,
  ],
  { env },
  {});

// Dropping a variable when every variable is accessible.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-env=*', '-e',
    `
    const assert = require('assert');
    assert.strictEqual(process.permission.has('env'), true);
    process.permission.drop('env', 'PERMISSION_ENV_ONE');
    assert.strictEqual(process.permission.has('env'), false);
    assert.strictEqual(process.permission.has('env', 'PERMISSION_ENV_ONE'), false);
    assert.strictEqual(process.env.PERMISSION_ENV_ONE, undefined);
    assert.strictEqual(process.permission.has('env', 'PERMISSION_ENV_TWO'), true);
    assert.strictEqual(process.env.PERMISSION_ENV_TWO, 'two');
    `,
  ],
  { env },
  {});
