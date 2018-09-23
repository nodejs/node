'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path'),
    Buffer = require('buffer').Buffer,
    fs = require('fs'),
    filepath = path.join(common.fixturesDir, 'x.txt'),
    fd = fs.openSync(filepath, 'r'),
    expected = 'xyz\n',
    bufferAsync = new Buffer(expected.length),
    bufferSync = new Buffer(expected.length),
    readCalled = 0;

fs.read(fd, bufferAsync, 0, expected.length, 0, function(err, bytesRead) {
  readCalled++;

  assert.equal(bytesRead, expected.length);
  assert.deepEqual(bufferAsync, new Buffer(expected));
});

var r = fs.readSync(fd, bufferSync, 0, expected.length, 0);
assert.deepEqual(bufferSync, new Buffer(expected));
assert.equal(r, expected.length);

process.on('exit', function() {
  assert.equal(readCalled, 1);
});
