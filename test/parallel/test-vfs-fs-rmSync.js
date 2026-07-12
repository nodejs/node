// Flags: --experimental-vfs
'use strict';

// fs.rmSync, fs.rmdirSync, and fs.unlinkSync dispatch to VFS.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-rmSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src/subdir', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
myVfs.writeFileSync('/src/subdir/inside.txt', 'inside');
myVfs.mkdirSync('/empty');
myVfs.mount(mountPoint);

// rmdirSync on an empty directory
fs.rmdirSync(path.join(mountPoint, 'empty'));
assert.strictEqual(fs.existsSync(path.join(mountPoint, 'empty')), false);

// unlinkSync on a file
fs.unlinkSync(path.join(mountPoint, 'src/hello.txt'));
assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')),
                   false);

// rmSync with force on a missing path is a no-op
fs.rmSync(path.join(mountPoint, 'missing'), { force: true });

// rmSync recursive on a non-empty directory tree
fs.rmSync(path.join(mountPoint, 'src'), { recursive: true });
assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src')), false);

myVfs.unmount();
