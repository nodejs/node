// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

// When --permission is used without --allow-env, all env vars should be denied
// (deny-by-default, consistent with other permission flags).
{
  assert.ok(!process.permission.has('env'));
}

{
  // Reading a denied variable should silently return undefined
  assert.strictEqual(process.env.HOME, undefined);
  assert.strictEqual(process.env.PATH, undefined);
}

{
  // Writing a denied variable should throw
  assert.throws(() => { process.env.__TEST_PERMISSION_ENV = 'test'; },
                { code: 'ERR_ACCESS_DENIED' });
}

{
  // Deleting a denied variable should throw
  assert.throws(() => { delete process.env.__TEST_PERMISSION_ENV; },
                { code: 'ERR_ACCESS_DENIED' });
}

{
  // Enumerating should return empty
  assert.strictEqual(Object.keys(process.env).length, 0);
}
