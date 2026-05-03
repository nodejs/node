// Flags: --experimental-vfs
'use strict';

// VFS readFile callback API.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/async-read.txt', 'async content');

myVfs.readFile('/async-read.txt', 'utf8', common.mustCall((err, data) => {
  assert.ifError(err);
  assert.strictEqual(data, 'async content');
}));

myVfs.readFile('/async-read.txt', common.mustCall((err, data) => {
  assert.ifError(err);
  assert.ok(Buffer.isBuffer(data));
  assert.strictEqual(data.toString(), 'async content');
}));

myVfs.readFile('/missing.txt', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
}));
