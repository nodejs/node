// Flags: --experimental-vfs
'use strict';

// fs.linkSync dispatches to VFS for hard links within the same mount.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-linkSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
myVfs.mount(mountPoint);

fs.linkSync(
  path.join(mountPoint, 'src/hello.txt'),
  path.join(mountPoint, 'src/hello-link.txt'),
);
assert.strictEqual(
  fs.readFileSync(path.join(mountPoint, 'src/hello-link.txt'), 'utf8'),
  'hello world',
);

myVfs.unmount();
