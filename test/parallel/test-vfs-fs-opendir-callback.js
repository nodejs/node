// Flags: --experimental-vfs
'use strict';

// fs.opendir callback dispatches through VFS, both via readSync() iteration
// and via async iteration.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const baseMountPoint = path.resolve('/tmp/vfs-opendir-cb-' + process.pid);
let counter = 0;
function mounted() {
  const mountPoint = baseMountPoint + '-' + (counter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello');
  myVfs.writeFileSync('/src/data.json', '{}');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// readSync() iteration
{
  const { myVfs, mountPoint } = mounted();
  fs.opendir(path.join(mountPoint, 'src'),
             common.mustSucceed((dir) => {
               const names = [];
               let entry;
               while ((entry = dir.readSync()) !== null) names.push(entry.name);
               dir.closeSync();
               assert.ok(names.includes('hello.txt'));
               myVfs.unmount();
             }));
}

// for-await-of iteration
{
  const { myVfs, mountPoint } = mounted();
  fs.opendir(path.join(mountPoint, 'src'),
             common.mustSucceed(async (dir) => {
               const names = [];
               for await (const entry of dir) names.push(entry.name);
               assert.ok(names.includes('hello.txt'));
               assert.ok(names.includes('data.json'));
               myVfs.unmount();
             }));
}
