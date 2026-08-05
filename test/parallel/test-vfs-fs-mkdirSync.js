// Flags: --experimental-vfs
'use strict';

// fs.mkdirSync dispatches to VFS, including the `recursive: true` form.

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();

const mountPoint = path.resolve('/tmp/vfs-mkdirSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.mount(mountPoint);

// Plain mkdir
fs.mkdirSync(path.join(mountPoint, 'src/d1'));
assert.strictEqual(
  fs.statSync(path.join(mountPoint, 'src/d1')).isDirectory(), true,
);

// Recursive mkdir creates intermediate directories and returns the first one,
// as a path below the mount point rather than a VFS-internal path.
const created = fs.mkdirSync(path.join(mountPoint, 'src/a/b/c'),
                             { recursive: true });
assert.strictEqual(created, path.join(mountPoint, 'src/a'));
assert.strictEqual(
  fs.statSync(path.join(mountPoint, 'src/a/b/c')).isDirectory(), true,
);

// Recursive mkdir with nothing left to create returns undefined.
assert.strictEqual(
  fs.mkdirSync(path.join(mountPoint, 'src/a/b'), { recursive: true }),
  undefined,
);

myVfs.unmount();

// The same holds for a mounted RealFSProvider, whose root must not leak into
// the returned path.
{
  const root = path.join(tmpdir.path, 'vfs-mkdirSync-real');
  fs.mkdirSync(root, { recursive: true });
  const realMountPoint = path.resolve('/tmp/vfs-mkdirSync-real-' + process.pid);
  const realVfs = vfs.create(new vfs.RealFSProvider(root));
  realVfs.mount(realMountPoint);

  const realCreated = fs.mkdirSync(path.join(realMountPoint, 'x/y/z'),
                                   { recursive: true });
  assert.strictEqual(realCreated, path.join(realMountPoint, 'x'));
  assert.strictEqual(fs.existsSync(path.join(root, 'x/y/z')), true);

  realVfs.unmount();
}
