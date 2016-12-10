'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');
const bufferAsync = Buffer.alloc(0);
const bufferSync = Buffer.alloc(0);

fs.read(fd, bufferAsync, 0, 0, 0, common.mustCall(function(err, bytesRead) {
  assert.equal(bytesRead, 0);
  assert.deepStrictEqual(bufferAsync, Buffer.alloc(0));
}));

const r = fs.readSync(fd, bufferSync, 0, 0, 0);
assert.deepStrictEqual(bufferSync, Buffer.alloc(0));
assert.equal(r, 0);
