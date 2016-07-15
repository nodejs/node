'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var Buffer = require('buffer').Buffer;
var fs = require('fs');
var fn = path.join(common.tmpDir, 'write.txt');
var fn2 = path.join(common.tmpDir, 'write2.txt');
var expected = 'ümlaut.';
var constants = fs.constants;

common.refreshTmpDir();

fs.open(fn, 'w', 0o644, common.mustCall(function(err, fd) {
  if (err) throw err;
  console.log('open done');
  fs.write(fd, '', 0, 'utf8', function(err, written) {
    assert.equal(0, written);
  });
  fs.write(fd, expected, 0, 'utf8', common.mustCall(function(err, written) {
    console.log('write done');
    if (err) throw err;
    assert.equal(Buffer.byteLength(expected), written);
    fs.closeSync(fd);
    const found = fs.readFileSync(fn, 'utf8');
    console.log('expected: "%s"', expected);
    console.log('found: "%s"', found);
    fs.unlinkSync(fn);
    assert.strictEqual(expected, found);
  }));
}));


fs.open(fn2, constants.O_CREAT | constants.O_WRONLY | constants.O_TRUNC, 0o644,
    common.mustCall(function(err, fd) {
      if (err) throw err;
      console.log('open done');
      fs.write(fd, '', 0, 'utf8', function(err, written) {
        assert.equal(0, written);
      });
      fs.write(fd, expected, 0, 'utf8', common.mustCall(function(err, written) {
        console.log('write done');
        if (err) throw err;
        assert.equal(Buffer.byteLength(expected), written);
        fs.closeSync(fd);
        const found = fs.readFileSync(fn2, 'utf8');
        console.log('expected: "%s"', expected);
        console.log('found: "%s"', found);
        fs.unlinkSync(fn2);
        assert.strictEqual(expected, found);
      }));
    }));
