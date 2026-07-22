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
  process.permission.drop('invalid-scope');
  process.permission.drop('invalid-scope', '/tmp');
}
