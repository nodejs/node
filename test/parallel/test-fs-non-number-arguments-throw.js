'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tempFile = path.join(common.tmpDir, 'fs-non-number-arguments-throw');

common.refreshTmpDir();
fs.writeFileSync(tempFile, 'abc\ndef');

// a sanity check when using numbers instead of strings
const sanity = 'def';
const saneEmitter = fs.createReadStream(tempFile, { start: 4, end: 6 });

common.expectsError(
  () => {
    fs.createReadStream(tempFile, { start: '4', end: 6 });
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

common.expectsError(
  () => {
    fs.createReadStream(tempFile, { start: 4, end: '6' });
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

common.expectsError(
  () => {
    fs.createWriteStream(tempFile, { start: '4' });
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

saneEmitter.on('data', common.mustCall(function(data) {
  assert.strictEqual(
    sanity, data.toString('utf8'),
    `read ${data.toString('utf8')} instead of ${sanity}`);
}));
