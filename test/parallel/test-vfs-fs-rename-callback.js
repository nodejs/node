// Flags: --experimental-vfs
'use strict';

// fs.rename and fs.copyFile callbacks dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const baseMountPoint = path.resolve('/tmp/vfs-rename-cb-' + process.pid);
let counter = 0;
function mounted() {
  const mountPoint = baseMountPoint + '-' + (counter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// rename (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.rename(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/renamed-cb.txt'),
    common.mustSucceed(() => {
      assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')),
                         false);
      assert.strictEqual(
        fs.readFileSync(path.join(mountPoint, 'src/renamed-cb.txt'), 'utf8'),
        'hello world',
      );
      myVfs.unmount();
    }),
  );
}

// copyFile (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.copyFile(
    path.join(mountPoint, 'src/hello.txt'),
    path.join(mountPoint, 'src/copy-cb.txt'),
    common.mustSucceed(() => {
      assert.strictEqual(
        fs.readFileSync(path.join(mountPoint, 'src/copy-cb.txt'), 'utf8'),
        'hello world',
      );
      myVfs.unmount();
    }),
  );
}
