'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';
const bufferAsync = new Buffer(expected.length);
const bufferSync = new Buffer(expected.length);
let readCalled = 0;

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
