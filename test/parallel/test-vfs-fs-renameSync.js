// Flags: --experimental-vfs
'use strict';

// fs.renameSync dispatches to VFS when both paths are within the same mount.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-renameSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
myVfs.mount(mountPoint);

fs.renameSync(
  path.join(mountPoint, 'src/hello.txt'),
  path.join(mountPoint, 'src/renamed.txt'),
);
assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')),
                   false);
assert.strictEqual(
  fs.readFileSync(path.join(mountPoint, 'src/renamed.txt'), 'utf8'),
  'hello world',
);

myVfs.unmount();
