'use strict';

// VFS stream constructors must validate start/end synchronously like real fs.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'abc');
myVfs.mount('/mnt_sv');

// ReadStream: start > end must throw ERR_OUT_OF_RANGE synchronously
assert.throws(
  () => fs.createReadStream('/mnt_sv/file.txt', { start: 2, end: 1 }),
  { code: 'ERR_OUT_OF_RANGE' },
);

// WriteStream: negative start must throw ERR_OUT_OF_RANGE synchronously
assert.throws(
  () => fs.createWriteStream('/mnt_sv/file.txt', { start: -1 }),
  { code: 'ERR_OUT_OF_RANGE' },
);

myVfs.unmount();
