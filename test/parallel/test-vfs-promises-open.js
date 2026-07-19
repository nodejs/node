// Flags: --experimental-vfs
'use strict';

// VFS promises.open returns a usable handle.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/hello.txt', 'hello from vfs');

(async () => {
  const fh = await myVfs.promises.open('/hello.txt', 'r');
  assert.ok(fh);
  assert.ok(typeof fh === 'number');
})().then(common.mustCall());
