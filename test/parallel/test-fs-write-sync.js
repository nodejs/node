'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const fn = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

const foo = 'foo';
const fd = fs.openSync(fn, 'w');

let written = fs.writeSync(fd, '');
assert.strictEqual(0, written);

fs.writeSync(fd, foo);

const bar = 'bár';
written = fs.writeSync(fd, Buffer.from(bar), 0, Buffer.byteLength(bar));
assert.ok(written > 3);
fs.closeSync(fd);

assert.strictEqual(fs.readFileSync(fn, 'utf8'), 'foobár');
