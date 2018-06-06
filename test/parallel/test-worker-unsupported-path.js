// Flags: --experimental-worker
'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

{
  const expectedErr = common.expectsError({
    code: 'ERR_WORKER_NEED_ABSOLUTE_PATH',
    type: TypeError
  }, 4);
  assert.throws(() => { new Worker('a.js'); }, expectedErr);
  assert.throws(() => { new Worker('b'); }, expectedErr);
  assert.throws(() => { new Worker('c/d.js'); }, expectedErr);
  assert.throws(() => { new Worker('a.mjs'); }, expectedErr);
}

{
  const expectedErr = common.expectsError({
    code: 'ERR_WORKER_UNSUPPORTED_EXTENSION',
    type: TypeError
  }, 3);
  assert.throws(() => { new Worker('/b'); }, expectedErr);
  assert.throws(() => { new Worker('/c.wasm'); }, expectedErr);
  assert.throws(() => { new Worker('/d.txt'); }, expectedErr);
}
