// Flags: --experimental-vfs
'use strict';

// fs.writeFileSync and fs.appendFileSync dispatch to VFS.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-writeFileSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.mount(mountPoint);

const target = path.join(mountPoint, 'src/new.txt');

fs.writeFileSync(target, 'fresh');
assert.strictEqual(fs.readFileSync(target, 'utf8'), 'fresh');

fs.appendFileSync(target, ' more');
assert.strictEqual(fs.readFileSync(target, 'utf8'), 'fresh more');

// Buffer input
fs.writeFileSync(target, Buffer.from('binary'));
assert.strictEqual(fs.readFileSync(target, 'utf8'), 'binary');

// writeFileSync via a VFS fd writes through the open descriptor.
{
  const fdTarget = path.join(mountPoint, 'src/fd.txt');
  const fd = fs.openSync(fdTarget, 'w+');
  try {
    fs.writeFileSync(fd, 'hello');
    fs.writeFileSync(fd, new Uint8Array(Buffer.from(' world')));
    assert.strictEqual(fs.readFileSync(fdTarget, 'utf8'), 'hello world');
  } finally {
    fs.closeSync(fd);
  }
}

// appendFileSync via a VFS fd follows normal fd write semantics.
{
  const fdTarget = path.join(mountPoint, 'src/append-fd.txt');
  fs.writeFileSync(fdTarget, 'start');
  const fd = fs.openSync(fdTarget, 'a');
  try {
    fs.appendFileSync(fd, ' end');
    assert.strictEqual(fs.readFileSync(fdTarget, 'utf8'), 'start end');
  } finally {
    fs.closeSync(fd);
  }
}

myVfs.unmount();

// writeFileSync via a RealFSProvider fd remains tied to the open descriptor
// after the backing path is renamed.
{
  const root = path.join('/tmp', 'vfs-real-writeFileSync-' + process.pid);
  const realMountPoint = path.join('/tmp', 'vfs-real-writeFileSync-mount-' + process.pid);
  fs.rmSync(root, { recursive: true, force: true });
  fs.rmSync(realMountPoint, { recursive: true, force: true });
  fs.mkdirSync(root, { recursive: true });
  fs.mkdirSync(realMountPoint, { recursive: true });

  const realVfs = vfs
    .create(new vfs.RealFSProvider(root), { emitExperimentalWarning: false })
    .mount(realMountPoint);
  try {
    const mountedFile = path.join(realMountPoint, 'a.txt');
    fs.writeFileSync(path.join(root, 'a.txt'), 'old');
    const fd = fs.openSync(mountedFile, 'r+');
    try {
      fs.renameSync(path.join(root, 'a.txt'), path.join(root, 'b.txt'));
      fs.writeFileSync(path.join(root, 'a.txt'), 'new');
      fs.writeFileSync(fd, 'updated');
      assert.strictEqual(fs.readFileSync(path.join(root, 'b.txt'), 'utf8'), 'updated');
      assert.strictEqual(fs.readFileSync(path.join(root, 'a.txt'), 'utf8'), 'new');
    } finally {
      fs.closeSync(fd);
    }
  } finally {
    realVfs.unmount();
    fs.rmSync(root, { recursive: true, force: true });
    fs.rmSync(realMountPoint, { recursive: true, force: true });
  }
}
