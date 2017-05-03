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

assert.throws(function() {
  fs.createReadStream(tempFile, { start: '4', end: 6 });
}, /^TypeError: "start" option must be a Number$/,
              "start as string didn't throw an error for createReadStream");

assert.throws(function() {
  fs.createReadStream(tempFile, { start: 4, end: '6' });
}, /^TypeError: "end" option must be a Number$/,
              "end as string didn't throw an error for createReadStream");

assert.throws(function() {
  fs.createWriteStream(tempFile, { start: '4' });
}, /^TypeError: "start" option must be a Number$/,
              "start as string didn't throw an error for createWriteStream");

saneEmitter.on('data', common.mustCall(function(data) {
  assert.strictEqual(
    sanity, data.toString('utf8'),
    `read ${data.toString('utf8')} instead of ${sanity}`);
}));
