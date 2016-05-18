'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var fn = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

var foo = 'foo';
var fd = fs.openSync(fn, 'w');

var written = fs.writeSync(fd, '');
assert.strictEqual(0, written);

fs.writeSync(fd, foo);

var bar = 'bár';
written = fs.writeSync(fd, Buffer.from(bar), 0, Buffer.byteLength(bar));
assert.ok(written > 3);
fs.closeSync(fd);

assert.equal(fs.readFileSync(fn), 'foobár');
