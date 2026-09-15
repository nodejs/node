// Flags: --experimental-vfs
'use strict';

// fs.createReadStream dispatches through VFS, including the emitted 'open'
// event with the bitmask-encoded virtual fd.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

// Whole-file read
{
  const { myVfs, mountPoint } = mounted();
  const chunks = [];
  const stream = fs.createReadStream(path.join(mountPoint, 'src/hello.txt'));
  stream.on('data', (chunk) => chunks.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), 'hello world');
    myVfs.unmount();
  }));
}

// Slice with start + end (inclusive)
{
  const { myVfs, mountPoint } = mounted();
  const chunks = [];
  const stream = fs.createReadStream(path.join(mountPoint, 'src/hello.txt'),
                                     { start: 0, end: 4 });
  assert.strictEqual(stream.path, path.join(mountPoint, 'src/hello.txt'));
  stream.on('data', (chunk) => chunks.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), 'hello');
    myVfs.unmount();
  }));
}

// 'open' event fires with a VFS fd
{
  const { myVfs, mountPoint } = mounted();
  const stream = fs.createReadStream(path.join(mountPoint, 'src/hello.txt'));
  stream.on('open', common.mustCall((fd) => {
    assert.notStrictEqual(fd & 0x40000000, 0);
  }));
  stream.on('end', common.mustCall(() => myVfs.unmount()));
  stream.resume();
}
