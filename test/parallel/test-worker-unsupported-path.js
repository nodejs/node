'use strict';

require('../common');
const path = require('path');
const assert = require('assert');
const { Worker } = require('worker_threads');

{
  const expectedErr = {
    code: 'ERR_WORKER_UNSUPPORTED_EXTENSION',
    name: 'TypeError'
  };
  assert.throws(() => { new Worker('/b'); }, expectedErr);
  assert.throws(() => { new Worker('/c.wasm'); }, expectedErr);
  assert.throws(() => { new Worker('/d.txt'); }, expectedErr);
}

{
  const expectedErr = {
    code: 'ERR_WORKER_PATH',
    name: 'TypeError'
  };
  const existingRelPathNoDot = path.relative('.', __filename);
  assert.throws(() => { new Worker(existingRelPathNoDot); }, expectedErr);
  assert.throws(() => { new Worker('relative_no_dot'); }, expectedErr);
  assert.throws(() => { new Worker('file:///file_url'); }, expectedErr);
  assert.throws(() => { new Worker('https://www.url.com'); }, expectedErr);
}
