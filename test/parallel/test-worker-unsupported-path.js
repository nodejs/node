'use strict';

const path = require('path');
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

{
  const expectedErr = common.expectsError({
    code: 'ERR_WORKER_UNSUPPORTED_EXTENSION',
    type: TypeError
  }, 3);
  assert.throws(() => { new Worker('/b'); }, expectedErr);
  assert.throws(() => { new Worker('/c.wasm'); }, expectedErr);
  assert.throws(() => { new Worker('/d.txt'); }, expectedErr);
}

{
  const expectedErr = common.expectsError({
    code: 'ERR_WORKER_PATH',
    type: TypeError
  }, 4);
  const existingRelPathNoDot = path.relative('.', __filename);
  assert.throws(() => { new Worker(existingRelPathNoDot); }, expectedErr);
  assert.throws(() => { new Worker('relative_no_dot'); }, expectedErr);
  assert.throws(() => { new Worker('file:///file_url'); }, expectedErr);
  assert.throws(() => { new Worker('https://www.url.com'); }, expectedErr);
}
