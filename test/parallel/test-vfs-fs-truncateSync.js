// Flags: --experimental-vfs
'use strict';

// fs.truncateSync dispatches to VFS and shrinks the file content.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
const mountPoint = myVfs.mount();

fs.truncateSync(path.join(mountPoint, 'src/hello.txt'), 5);
assert.strictEqual(
  fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'),
  'hello',
);

myVfs.unmount();
