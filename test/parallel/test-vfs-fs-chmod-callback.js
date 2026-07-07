// Flags: --experimental-vfs
'use strict';

// fs.chmod, fs.chown, fs.lchown, fs.utimes, and fs.lutimes callbacks dispatch
// through VFS.

const common = require('../common');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
const mountPoint = myVfs.mount();

const target = path.join(mountPoint, 'src/hello.txt');
const uid = process.getuid?.() ?? 0;
const gid = process.getgid?.() ?? 0;
const now = new Date();

fs.chmod(target, 0o644, common.mustSucceed(() => {
  fs.chown(target, uid, gid, common.mustSucceed(() => {
    fs.lchown(target, uid, gid, common.mustSucceed(() => {
      fs.utimes(target, now, now, common.mustSucceed(() => {
        fs.lutimes(target, now, now, common.mustSucceed(() => {
          myVfs.unmount();
        }));
      }));
    }));
  }));
}));
