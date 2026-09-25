// Flags: --experimental-vfs
'use strict';

// fs.writeFile and fs.appendFile callbacks dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

// writeFile (cb)
{
  const { myVfs, mountPoint } = mounted();
  const target = path.join(mountPoint, 'src/cb-w.txt');
  fs.writeFile(target, 'cbw', common.mustSucceed(() => {
    assert.strictEqual(fs.readFileSync(target, 'utf8'), 'cbw');
    myVfs.unmount();
  }));
}

// appendFile (cb)
{
  const { myVfs, mountPoint } = mounted();
  const target = path.join(mountPoint, 'src/cb-a.txt');
  fs.writeFileSync(target, 'base');
  fs.appendFile(target, ' more', common.mustSucceed(() => {
    assert.strictEqual(fs.readFileSync(target, 'utf8'), 'base more');
    myVfs.unmount();
  }));
}
