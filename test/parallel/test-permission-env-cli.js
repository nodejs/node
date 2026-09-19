'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSyncAndAssert, spawnSyncAndExit } = require('../common/child_process');

for (const value of ['A*B', '*A', 'A=B', 'A,B*C']) {
  spawnSyncAndExit(
    process.execPath,
    ['--permission', `--allow-env=${value}`, '-e', ''],
    {
      status: 9,
      signal: null,
      stderr: /--allow-env must be '\*', a variable name, or a variable name prefix followed by '\*'/,
    });
}

// --allow-env is accepted in NODE_OPTIONS.
if (!process.config.variables.node_without_node_options) {
  spawnSyncAndAssert(
    process.execPath,
    ['--permission', '-p', 'process.env.PERMISSION_ENV_ALLOWED'],
    {
      env: {
        ...process.env,
        NODE_OPTIONS: '--allow-env=PERMISSION_ENV_ALLOWED',
        PERMISSION_ENV_ALLOWED: 'allowed',
      },
    },
    {
      stdout(output) {
        assert.strictEqual(output.trim(), 'allowed');
      },
    });
}
