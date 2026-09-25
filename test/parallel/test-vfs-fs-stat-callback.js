// Flags: --experimental-vfs
'use strict';

// fs.stat and fs.lstat callbacks dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

// stat (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.stat(path.join(mountPoint, 'src/hello.txt'),
          common.mustSucceed((s) => {
            assert.strictEqual(s.isFile(), true);
            assert.strictEqual(s.size, 11);
            myVfs.unmount();
          }));
}

// lstat (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.lstat(path.join(mountPoint, 'src/hello.txt'),
           common.mustSucceed((s) => {
             assert.strictEqual(s.isFile(), true);
             myVfs.unmount();
           }));
}
