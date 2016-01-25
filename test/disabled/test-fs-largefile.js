'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path'),
    fs = require('fs'),
    filepath = path.join(common.tmpDir, 'large.txt'),
    fd = fs.openSync(filepath, 'w+'),
    offset = 5 * 1024 * 1024 * 1024, // 5GB
    message = 'Large File';

fs.truncateSync(fd, offset);
assert.equal(fs.statSync(filepath).size, offset);
var writeBuf = Buffer.from(message);
fs.writeSync(fd, writeBuf, 0, writeBuf.length, offset);
var readBuf = Buffer.allocUnsafe(writeBuf.length);
fs.readSync(fd, readBuf, 0, readBuf.length, offset);
assert.equal(readBuf.toString(), message);
fs.readSync(fd, readBuf, 0, 1, 0);
assert.equal(readBuf[0], 0);

var exceptionRaised = false;
try {
  fs.writeSync(fd, writeBuf, 0, writeBuf.length, 42.000001);
} catch (err) {
  console.log(err);
  exceptionRaised = true;
  assert.equal(err.message, 'Not an integer');
}
assert.ok(exceptionRaised);
fs.close(fd);

process.on('exit', function() {
  fs.unlinkSync(filepath);
});
