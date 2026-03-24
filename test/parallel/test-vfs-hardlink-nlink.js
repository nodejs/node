'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-nlink-' + process.pid);

const myVfs = vfs.create();
myVfs.writeFileSync('/src.txt', 'content');
myVfs.mount(mountPoint);

const srcPath = path.join(mountPoint, 'src.txt');
const dstPath = path.join(mountPoint, 'dst.txt');

// Before linking, nlink should be 1
{
  const stat = fs.statSync(srcPath);
  assert.strictEqual(stat.nlink, 1);
}

// After linking, both paths should report nlink === 2
fs.linkSync(srcPath, dstPath);
{
  const srcStat = fs.statSync(srcPath);
  const dstStat = fs.statSync(dstPath);
  assert.strictEqual(srcStat.nlink, 2);
  assert.strictEqual(dstStat.nlink, 2);
}

// After unlinking one, the other should report nlink === 1
fs.unlinkSync(dstPath);
{
  const srcStat = fs.statSync(srcPath);
  assert.strictEqual(srcStat.nlink, 1);
}

myVfs.unmount();
