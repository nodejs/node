// Flags: --experimental-vfs
'use strict';

// fs.symlink and fs.readlink callbacks dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-symlink-cb-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
myVfs.mount(mountPoint);

fs.symlink('hello.txt', path.join(mountPoint, 'src/lnk.txt'),
           common.mustSucceed(() => {
             fs.readlink(path.join(mountPoint, 'src/lnk.txt'),
                         common.mustSucceed((target) => {
                           assert.strictEqual(target, 'hello.txt');
                           myVfs.unmount();
                         }));
           }));
