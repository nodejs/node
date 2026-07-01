// Flags: --permission --allow-fs-read=* --allow-env=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

// With --allow-env=* all env vars should be accessible
{
  assert.ok(process.permission.has('env'));
  // Should not throw for any env var
  assert.ok(process.env.HOME !== undefined);
  assert.ok(process.env.PATH !== undefined);
  process.env.TEST_PERMISSION_VAR = 'test';
  assert.strictEqual(process.env.TEST_PERMISSION_VAR, 'test');
  delete process.env.TEST_PERMISSION_VAR;
  assert.strictEqual(process.env.TEST_PERMISSION_VAR, undefined);
}
