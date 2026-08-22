'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const prefixString = path.join(tmpdir.path, 'buffer-');
const prefixBuffer = Buffer.from(prefixString);

// 1. Test Sync API
const resultSync = fs.mkdtempSync(prefixBuffer);
assert.strictEqual(Buffer.isBuffer(resultSync), true);

// 2. Test Callback API
fs.mkdtemp(prefixBuffer, common.mustSucceed((resultCb) => {
  assert.strictEqual(Buffer.isBuffer(resultCb), true);
}));

// 3. Test Promises API
fs.promises.mkdtemp(prefixBuffer)
  .then(common.mustCall((resultPromise) => {
    assert.strictEqual(Buffer.isBuffer(resultPromise), true);
  }));
