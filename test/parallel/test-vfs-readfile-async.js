'use strict';

// fs.readFile on a VFS path must use the async handler, not sync.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/async-read.txt', 'async content');
myVfs.mount('/mnt_asyncrd');

fs.readFile('/mnt_asyncrd/async-read.txt', 'utf8', common.mustCall((err, data) => {
  assert.ifError(err);
  assert.strictEqual(data, 'async content');
  myVfs.unmount();
}));
