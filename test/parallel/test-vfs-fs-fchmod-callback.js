// Flags: --experimental-vfs
'use strict';

// fs.fchmod, fs.fchown, fs.futimes, fs.fdatasync, and fs.fsync callbacks
// short-circuit through VFS as no-ops on virtual fds.

const common = require('../common');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-fchmod-cb-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
myVfs.mount(mountPoint);

const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r+');
const uid = process.getuid?.() ?? 0;
const gid = process.getgid?.() ?? 0;
const now = new Date();

fs.fchmod(fd, 0o644, common.mustSucceed(() => {
  fs.fchown(fd, uid, gid, common.mustSucceed(() => {
    fs.futimes(fd, now, now, common.mustSucceed(() => {
      fs.fdatasync(fd, common.mustSucceed(() => {
        fs.fsync(fd, common.mustSucceed(() => {
          fs.closeSync(fd);
          myVfs.unmount();
        }));
      }));
    }));
  }));
}));
