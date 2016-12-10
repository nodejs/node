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

fs.read(fd,
        bufferAsync,
        0,
        expected.length,
        0,
        common.mustCall(function(err, bytesRead) {
          assert.equal(bytesRead, expected.length);
          assert.deepStrictEqual(bufferAsync, Buffer.from(expected));
        }));

var r = fs.readSync(fd, bufferSync, 0, expected.length, 0);
assert.deepStrictEqual(bufferSync, Buffer.from(expected));
assert.equal(r, expected.length);
