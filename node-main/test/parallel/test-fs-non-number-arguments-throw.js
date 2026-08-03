'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const tempFile = tmpdir.resolve('fs-non-number-arguments-throw');

tmpdir.refresh();
fs.writeFileSync(tempFile, 'abc\ndef');

// A sanity check when using numbers instead of strings
const sanity = 'def';
const saneEmitter = fs.createReadStream(tempFile, { start: 4, end: 6 });

assert.throws(
  () => {
    fs.createReadStream(tempFile, { start: '4', end: 6 });
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });

assert.throws(
  () => {
    fs.createReadStream(tempFile, { start: 4, end: '6' });
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });

assert.throws(
  () => {
    fs.createWriteStream(tempFile, { start: '4' });
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });

saneEmitter.on('data', common.mustCall(function(data) {
  assert.strictEqual(
    sanity, data.toString('utf8'),
    `read ${data.toString('utf8')} instead of ${sanity}`);
}));
