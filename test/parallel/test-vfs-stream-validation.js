// Flags: --experimental-vfs
'use strict';

// VFS stream constructors must validate start/end synchronously.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'abc');

// ReadStream: start > end must throw ERR_OUT_OF_RANGE synchronously
assert.throws(
  () => myVfs.createReadStream('/file.txt', { start: 2, end: 1 }),
  { code: 'ERR_OUT_OF_RANGE' },
);

// ReadStream: negative start
assert.throws(
  () => myVfs.createReadStream('/file.txt', { start: -1 }),
  { code: 'ERR_OUT_OF_RANGE' },
);

// ReadStream: negative end
assert.throws(
  () => myVfs.createReadStream('/file.txt', { end: -1 }),
  { code: 'ERR_OUT_OF_RANGE' },
);
