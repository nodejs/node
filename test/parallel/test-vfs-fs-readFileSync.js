// Flags: --experimental-vfs
'use strict';

// fs.readFileSync dispatches to VFS for both string paths and VFS-owned fds.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
const mountPoint = myVfs.mount();

// Default (buffer) result
{
  const buf = fs.readFileSync(path.join(mountPoint, 'src/hello.txt'));
  assert.ok(Buffer.isBuffer(buf));
  assert.strictEqual(buf.toString(), 'hello world');
}

// utf8 encoding -> string result
assert.strictEqual(
  fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'),
  'hello world',
);

// Encoding via options object
assert.strictEqual(
  fs.readFileSync(path.join(mountPoint, 'src/hello.txt'),
                  { encoding: 'utf8' }),
  'hello world',
);

// readFileSync via a VFS fd
{
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  assert.strictEqual(fs.readFileSync(fd, 'utf8'), 'hello world');
  fs.closeSync(fd);
}

myVfs.unmount();

// readFileSync via a RealFSProvider fd remains usable after the backing path
// is renamed.
{
  const root = path.join('/tmp', 'vfs-real-readFileSync-' + process.pid);
  fs.rmSync(root, { recursive: true, force: true });
  fs.mkdirSync(root, { recursive: true });

  const realVfs = vfs.create(new vfs.RealFSProvider(root),
                             { emitExperimentalWarning: false });
  const realMountPoint = realVfs.mount();
  try {
    fs.writeFileSync(path.join(root, 'a.txt'), 'still readable');
    const fd = fs.openSync(path.join(realMountPoint, 'a.txt'), 'r');
    try {
      fs.renameSync(path.join(root, 'a.txt'), path.join(root, 'b.txt'));
      assert.strictEqual(fs.readFileSync(fd, 'utf8'), 'still readable');
    } finally {
      fs.closeSync(fd);
    }
  } finally {
    realVfs.unmount();
    fs.rmSync(root, { recursive: true, force: true });
  }
}
