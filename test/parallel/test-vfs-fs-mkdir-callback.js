// Flags: --experimental-vfs
'use strict';

// fs.mkdir, fs.rmdir, fs.rm, and fs.unlink callbacks dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const baseMountPoint = path.resolve('/tmp/vfs-mkdir-cb-' + process.pid);
let counter = 0;
function mounted() {
  const mountPoint = baseMountPoint + '-' + (counter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// mkdir (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.mkdir(path.join(mountPoint, 'src/cb-d'), common.mustSucceed(() => {
    assert.strictEqual(
      fs.statSync(path.join(mountPoint, 'src/cb-d')).isDirectory(), true,
    );
    myVfs.unmount();
  }));
}

// rmdir (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.mkdirSync(path.join(mountPoint, 'src/empty'));
  fs.rmdir(path.join(mountPoint, 'src/empty'), common.mustSucceed(() => {
    assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/empty')),
                       false);
    myVfs.unmount();
  }));
}

// rm (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.rm(path.join(mountPoint, 'src/hello.txt'),
        common.mustSucceed(() => {
          assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')),
                             false);
          myVfs.unmount();
        }));
}

// unlink (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.unlink(path.join(mountPoint, 'src/hello.txt'),
            common.mustSucceed(() => {
              assert.strictEqual(
                fs.existsSync(path.join(mountPoint, 'src/hello.txt')), false,
              );
              myVfs.unmount();
            }));
}
