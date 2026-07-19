// Flags: --experimental-vfs
'use strict';

// fs.openAsBlob dispatches to VFS and returns a Blob over the virtual file.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-openAsBlob-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
myVfs.mount(mountPoint);

fs.openAsBlob(path.join(mountPoint, 'src/hello.txt'))
  .then(async (blob) => {
    assert.ok(blob instanceof Blob);
    assert.strictEqual(blob.size, 11);
    assert.strictEqual(await blob.text(), 'hello world');
    myVfs.unmount();
  })
  .then(common.mustCall());
