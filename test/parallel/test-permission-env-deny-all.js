// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

{
  assert.ok(!process.permission.has('env'));
}

{
  assert.strictEqual(process.env.HOME, undefined);
  assert.strictEqual(process.env.PATH, undefined);
}

{
  assert.throws(() => {
    process.env.TEST_VAR = 'value';
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'EnvVar',
  }));
}

{
  const keys = Object.keys(process.env);
  assert.strictEqual(keys.length, 0);
}
