'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const prefixString = tmpdir.resolve('buffer-');
const prefixBuffer = Buffer.from(prefixString);

// 1. Test Sync API
const resultSync = fs.mkdtempSync(prefixBuffer);
assert.strictEqual(Buffer.isBuffer(resultSync), true);

// 2. Test Callback API
fs.mkdtemp(prefixBuffer, common.mustSucceed((result) => {
  assert.strictEqual(Buffer.isBuffer(result), true);
}));

// 3. Test Promises API
fs.promises.mkdtemp(prefixBuffer)
  .then(common.mustCall((resultPromise) => {
    assert.strictEqual(Buffer.isBuffer(resultPromise), true);
  }));
