// Flags: --experimental-vfs
'use strict';

// VFS readFile callback API.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/async-read.txt', 'async content');

myVfs.readFile('/async-read.txt', 'utf8', common.mustSucceed((data) => {
  assert.strictEqual(data, 'async content');
}));

myVfs.readFile('/async-read.txt', common.mustSucceed((data) => {
  assert.ok(Buffer.isBuffer(data));
  assert.strictEqual(data.toString(), 'async content');
}));

myVfs.readFile('/missing.txt', common.expectsError({
  code: 'ENOENT',
}));
