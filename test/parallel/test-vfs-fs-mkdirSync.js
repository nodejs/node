// Flags: --experimental-vfs
'use strict';

// fs.mkdirSync dispatches to VFS, including the `recursive: true` form.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-mkdirSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.mount(mountPoint);

// Plain mkdir
fs.mkdirSync(path.join(mountPoint, 'src/d1'));
assert.strictEqual(
  fs.statSync(path.join(mountPoint, 'src/d1')).isDirectory(), true,
);

// Recursive mkdir creates intermediate directories and returns the first one
const created = fs.mkdirSync(path.join(mountPoint, 'src/a/b/c'),
                             { recursive: true });
assert.ok(created !== undefined);
assert.strictEqual(
  fs.statSync(path.join(mountPoint, 'src/a/b/c')).isDirectory(), true,
);

myVfs.unmount();
