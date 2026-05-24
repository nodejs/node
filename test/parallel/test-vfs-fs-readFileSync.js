// Flags: --experimental-vfs
'use strict';

// fs.readFileSync dispatches to VFS for both string paths and VFS-owned fds.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-readFileSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
myVfs.mount(mountPoint);

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
