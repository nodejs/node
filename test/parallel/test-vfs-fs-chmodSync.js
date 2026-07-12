// Flags: --experimental-vfs
'use strict';

// fs.chmodSync, fs.chownSync, fs.lchownSync, fs.utimesSync, and
// fs.lutimesSync dispatch to VFS. The MemoryProvider accepts these calls as
// metadata mutations without throwing.

require('../common');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-chmodSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
myVfs.mount(mountPoint);

const target = path.join(mountPoint, 'src/hello.txt');
const uid = process.getuid?.() ?? 0;
const gid = process.getgid?.() ?? 0;
const now = new Date();

fs.chmodSync(target, 0o644);
fs.chownSync(target, uid, gid);
fs.lchownSync(target, uid, gid);
fs.utimesSync(target, now, now);
fs.lutimesSync(target, now, now);

myVfs.unmount();
