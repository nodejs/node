// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

// When --permission is set without --allow-env, all env access is denied
{
  assert.ok(!process.permission.has('env'));
}

// Reading any env var should return undefined (silently denied)
{
  assert.strictEqual(process.env.HOME, undefined);
  assert.strictEqual(process.env.PATH, undefined);
}

// Setting any env var should throw
{
  assert.throws(() => {
    process.env.TEST_VAR = 'value';
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'EnvVar',
  }));
}

// Enumerating should return empty
{
  const keys = Object.keys(process.env);
  assert.strictEqual(keys.length, 0);
}
