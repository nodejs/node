'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';
const bufferAsync = Buffer.allocUnsafe(expected.length);
const bufferSync = Buffer.allocUnsafe(expected.length);
let readCalled = 0;

fs.read(fd, bufferAsync, 0, expected.length, 0, function(err, bytesRead) {
  readCalled++;

  assert.equal(bytesRead, expected.length);
  assert.deepEqual(bufferAsync, Buffer.from(expected));
});

var r = fs.readSync(fd, bufferSync, 0, expected.length, 0);
assert.deepEqual(bufferSync, Buffer.from(expected));
assert.equal(r, expected.length);

process.on('exit', function() {
  assert.equal(readCalled, 1);
});
