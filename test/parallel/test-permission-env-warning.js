'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');

const env = {
  ...process.env,
  PERMISSION_ENV_SECRET: 'secret',
};

// Reading a variable that was removed at startup publishes a denial every
// time, and warns once.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '-e',
    `
    const assert = require('assert');
    const dc = require('diagnostics_channel');
    const events = [];
    dc.subscribe('node:permission-model:env', (message) => {
      events.push([message.permission, message.resource]);
    });

    assert.strictEqual(process.env.PERMISSION_ENV_SECRET, undefined);
    assert.strictEqual(process.env.PERMISSION_ENV_SECRET, undefined);
    assert.strictEqual('PERMISSION_ENV_SECRET' in process.env, false);
    // Variables that were never set are not reported.
    assert.strictEqual(process.env.PERMISSION_ENV_NEVER_SET, undefined);

    assert.deepStrictEqual(events, [
      ['Env', 'PERMISSION_ENV_SECRET'],
      ['Env', 'PERMISSION_ENV_SECRET'],
      ['Env', 'PERMISSION_ENV_SECRET'],
    ]);
    `,
  ],
  { env },
  {
    stderr(output) {
      const warnings = output.match(
        /Warning: The permission model removed the environment variable "PERMISSION_ENV_SECRET" at startup\. Use --allow-env to manage permissions\./g);
      assert.strictEqual(warnings?.length, 1, output);
      assert.doesNotMatch(output, /PERMISSION_ENV_NEVER_SET/);
    },
  });

// --no-warnings silences the warning.
spawnSyncAndAssert(
  process.execPath,
  ['--permission', '--no-warnings', '-e', 'process.env.PERMISSION_ENV_SECRET'],
  { env },
  { stderr: '' });

// In audit mode nothing is removed. Accesses to variables that --allow-env
// does not grant access to are published instead, without a warning.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission-audit', '--allow-env=PERMISSION_ENV_ALLOWED', '-e',
    `
    const assert = require('assert');
    const dc = require('diagnostics_channel');
    const events = [];
    dc.subscribe('node:permission-model:env', (message) => {
      events.push(message.resource);
    });

    assert.strictEqual(process.env.PERMISSION_ENV_SECRET, 'secret');
    assert.strictEqual('PERMISSION_ENV_SECRET' in process.env, true);
    assert.strictEqual(process.env.PERMISSION_ENV_ALLOWED, 'allowed');
    process.env.TZ;
    // Variables created at runtime would not have been removed either.
    process.env.PERMISSION_ENV_RUNTIME = 'runtime';
    assert.strictEqual(process.env.PERMISSION_ENV_RUNTIME, 'runtime');

    assert.deepStrictEqual(events, [
      'PERMISSION_ENV_SECRET',
      'PERMISSION_ENV_SECRET',
    ]);
    `,
  ],
  { env: { ...env, PERMISSION_ENV_ALLOWED: 'allowed' } },
  {
    stderr(output) {
      assert.doesNotMatch(output, /removed the environment variable/);
    },
  });
