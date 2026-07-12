// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsp = require('fs/promises');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-lchown-' + process.pid);
const myVfs = vfs.create();
myVfs.writeFileSync('/sync-target.txt', 'target');
myVfs.symlinkSync('/sync-target.txt', '/sync-link.txt');
myVfs.writeFileSync('/async-target.txt', 'target');
myVfs.symlinkSync('/async-target.txt', '/async-link.txt');
myVfs.writeFileSync('/promise-target.txt', 'target');
myVfs.symlinkSync('/promise-target.txt', '/promise-link.txt');
myVfs.mount(mountPoint);

function assertOwnership(filePath, uid, gid) {
  const stats = fs.lstatSync(path.join(mountPoint, filePath));
  assert.strictEqual(stats.uid, uid);
  assert.strictEqual(stats.gid, gid);
}

(async () => {
  try {
    fs.lchownSync(path.join(mountPoint, 'sync-link.txt'), 123, 456);
    assertOwnership('/sync-target.txt', 0, 0);
    assertOwnership('/sync-link.txt', 123, 456);

    await new Promise((resolve, reject) => {
      fs.lchown(path.join(mountPoint, 'async-link.txt'), 234, 567, (err) => {
        if (err) reject(err);
        else resolve();
      });
    });
    assertOwnership('/async-target.txt', 0, 0);
    assertOwnership('/async-link.txt', 234, 567);

    await fsp.lchown(path.join(mountPoint, 'promise-link.txt'), 345, 678);
    assertOwnership('/promise-target.txt', 0, 0);
    assertOwnership('/promise-link.txt', 345, 678);
  } finally {
    myVfs.unmount();
  }
})().then(common.mustCall());
