// Flags: --experimental-vfs
'use strict';

// fs.opendirSync dispatches to VFS and returns a Dir-like iterable.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-opendirSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src/subdir', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
myVfs.writeFileSync('/src/data.json', '{}');
myVfs.mount(mountPoint);

const dir = fs.opendirSync(path.join(mountPoint, 'src'));
const names = [];
let entry;
while ((entry = dir.readSync()) !== null) names.push(entry.name);
dir.closeSync();

assert.ok(names.includes('hello.txt'));
assert.ok(names.includes('data.json'));
assert.ok(names.includes('subdir'));

myVfs.unmount();
