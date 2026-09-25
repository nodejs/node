// Flags: --experimental-vfs
'use strict';

// fs.createWriteStream dispatches through VFS, exposes a `path` property and
// emits an 'open' event with the bitmask-encoded virtual fd.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

// Basic write
{
  const { myVfs, mountPoint } = mounted();
  const target = path.join(mountPoint, 'src/sw.txt');
  const stream = fs.createWriteStream(target);
  stream.write('stream ');
  stream.end('data', common.mustCall(() => {
    assert.strictEqual(fs.readFileSync(target, 'utf8'), 'stream data');
    myVfs.unmount();
  }));
}

// Path getter + 'open' event with a VFS fd
{
  const { myVfs, mountPoint } = mounted();
  const target = path.join(mountPoint, 'src/ws-open.txt');
  const stream = fs.createWriteStream(target);
  assert.strictEqual(stream.path, target);
  stream.on('open', common.mustCall((fd) => {
    assert.notStrictEqual(fd & 0x40000000, 0);
  }));
  stream.end('done', common.mustCall(() => myVfs.unmount()));
}
