// Flags: --experimental-vfs
'use strict';

// fs.symlinkSync and fs.readlinkSync dispatch through VFS, including the
// buffer-encoding variant of readlinkSync.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
const mountPoint = myVfs.mount();

fs.symlinkSync('hello.txt', path.join(mountPoint, 'src/link.txt'));
assert.strictEqual(
  fs.readlinkSync(path.join(mountPoint, 'src/link.txt')),
  'hello.txt',
);

const buf = fs.readlinkSync(path.join(mountPoint, 'src/link.txt'),
                            { encoding: 'buffer' });
assert.ok(Buffer.isBuffer(buf));
assert.strictEqual(buf.toString(), 'hello.txt');

myVfs.unmount();
