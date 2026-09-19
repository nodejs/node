'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const { spawnSyncAndAssert } = require('../common/child_process');

const env = {
  ...process.env,
  PERMISSION_ENV_SECRET: 'secret',
  PERMISSION_ENV_ALLOWED: 'allowed',
};

// The environment of a child spawned by a process that enforces the
// permission model already reflects --allow-env, so the child gets access to
// all of it: including variables the parent added, and ones added for the
// child only. It still cannot see what the parent removed.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-child-process', '--no-warnings',
    '--allow-env=PERMISSION_ENV_ALLOWED', '-e',
    `
    const assert = require('assert');
    const { spawnSync } = require('child_process');
    process.env.PERMISSION_ENV_RUNTIME = 'runtime';
    const { status, stdout, stderr } = spawnSync(process.execPath, [
      '--no-warnings', '-p',
      'JSON.stringify([' +
        'process.env.PERMISSION_ENV_ALLOWED, ' +
        'process.env.PERMISSION_ENV_RUNTIME, ' +
        'process.env.PERMISSION_ENV_CHILD, ' +
        'process.env.PERMISSION_ENV_SECRET, ' +
        'process.permission.has("env")])',
    ], {
      env: { ...process.env, PERMISSION_ENV_CHILD: 'child' },
    });
    assert.strictEqual(status, 0, stderr.toString());
    assert.deepStrictEqual(
      JSON.parse(stdout),
      ['allowed', 'runtime', 'child', null, true]);
    `,
  ],
  { env },
  {});

// In audit mode nothing was removed, so children audit the same policy.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission-audit', '--allow-child-process', '--no-warnings',
    '--allow-env=PERMISSION_ENV_ALLOWED', '-e',
    `
    const assert = require('assert');
    const { spawnSync } = require('child_process');
    const { status, stdout, stderr } = spawnSync(process.execPath, [
      '--no-warnings', '-p',
      'JSON.stringify([' +
        'process.permission.has("env"), ' +
        'process.permission.has("env", "PERMISSION_ENV_ALLOWED"), ' +
        'process.permission.has("env", "PERMISSION_ENV_SECRET"), ' +
        'process.env.PERMISSION_ENV_SECRET])',
    ]);
    assert.strictEqual(status, 0, stderr.toString());
    assert.deepStrictEqual(JSON.parse(stdout), [false, true, false, 'secret']);
    `,
  ],
  { env },
  {});
