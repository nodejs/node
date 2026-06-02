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

myVfs.unmount();
