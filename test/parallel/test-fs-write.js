'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const fn = path.join(common.tmpDir, 'write.txt');
const fn2 = path.join(common.tmpDir, 'write2.txt');
const expected = 'ümlaut.';
const constants = fs.constants;

common.refreshTmpDir();

fs.open(fn, 'w', 0o644, common.mustCall(function(err, fd) {
  if (err) throw err;
  console.log('open done');
  fs.write(fd, '', 0, 'utf8', function(err, written) {
    assert.strictEqual(0, written);
  });
  fs.write(fd, expected, 0, 'utf8', common.mustCall(function(err, written) {
    console.log('write done');
    if (err) throw err;
    assert.strictEqual(Buffer.byteLength(expected), written);
    fs.closeSync(fd);
    const found = fs.readFileSync(fn, 'utf8');
    console.log('expected: "%s"', expected);
    console.log('found: "%s"', found);
    fs.unlinkSync(fn);
    assert.strictEqual(expected, found);
  }));
}));


fs.open(fn2, constants.O_CREAT | constants.O_WRONLY | constants.O_TRUNC, 0o644,
        common.mustCall((err, fd) => {
          if (err) throw err;
          console.log('open done');
          fs.write(fd, '', 0, 'utf8', (err, written) => {
            assert.strictEqual(0, written);
          });
          fs.write(fd, expected, 0, 'utf8', common.mustCall((err, written) => {
            console.log('write done');
            if (err) throw err;
            assert.strictEqual(Buffer.byteLength(expected), written);
            fs.closeSync(fd);
            const found = fs.readFileSync(fn2, 'utf8');
            console.log('expected: "%s"', expected);
            console.log('found: "%s"', found);
            fs.unlinkSync(fn2);
            assert.strictEqual(expected, found);
          }));
        }));
