// Flags: --permission --allow-fs-read=* --allow-child-process
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

// When --permission is used without --allow-env, env vars should be
// freely accessible (backward compatible behavior).
{
  assert.ok(process.permission.has('env'));
}

{
  // Environment variables should be readable
  assert.ok(typeof process.env.HOME === 'string' || process.env.HOME === undefined);
}

{
  // Setting env vars should work
  process.env.__TEST_PERMISSION_ENV = 'test';
  assert.strictEqual(process.env.__TEST_PERMISSION_ENV, 'test');
  delete process.env.__TEST_PERMISSION_ENV;
}

{
  // Object.keys should return env vars
  const keys = Object.keys(process.env);
  assert.ok(keys.length > 0);
}

// Test that restriction activates when --allow-env is explicitly used
{
  const { spawnSync } = require('child_process');
  const { status, stderr } = spawnSync(process.execPath, [
    '--permission',
    '--allow-fs-read=*',
    '--allow-env=__NONEXISTENT_VAR__',
    '-e',
    `
    const assert = require('assert');
    assert.ok(!process.permission.has('env'));
    assert.strictEqual(process.env.HOME, undefined);
    assert.strictEqual(process.env.PATH, undefined);
    assert.throws(() => { process.env.X = '1'; }, { code: 'ERR_ACCESS_DENIED' });
    assert.strictEqual(Object.keys(process.env).length, 0);
    `,
  ]);
  assert.strictEqual(status, 0, `child stderr: ${stderr}`);
}
