// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

{
  assert.throws(() => {
    process.permission.drop(null);
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "scope" argument must be of type string. Received null',
  }));

  assert.throws(() => {
    process.permission.drop(123);
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
  }));
}

{
  assert.throws(() => {
    process.permission.drop('fs.read', {});
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
  }));

  assert.throws(() => {
    process.permission.drop('fs.read', 123);
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
  }));
}

{
  // An empty reference must not drop the whole scope.
  for (const scope of ['fs.read', 'fs.write', 'child', 'env']) {
    for (const reference of ['', new Uint8Array(0), Buffer.alloc(0)]) {
      assert.throws(() => {
        process.permission.drop(scope, reference);
      }, common.expectsError({
        code: 'ERR_INVALID_ARG_VALUE',
      }));
    }
  }

  assert.strictEqual(process.permission.has('fs.read'), true);
  assert.strictEqual(process.permission.has('child'), false);

  process.permission.drop('child', null);
  process.permission.drop('child', undefined);
}

{
  process.permission.drop('invalid-scope');
  process.permission.drop('invalid-scope', '/tmp');
}
