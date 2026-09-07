// Flags: --experimental-vfs
'use strict';

// fs.copyFileSync dispatches to VFS when both paths are within the same mount.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
const mountPoint = myVfs.mount();

fs.copyFileSync(
  path.join(mountPoint, 'src/hello.txt'),
  path.join(mountPoint, 'src/copy.txt'),
);
assert.strictEqual(
  fs.readFileSync(path.join(mountPoint, 'src/copy.txt'), 'utf8'),
  'hello world',
);

myVfs.unmount();
