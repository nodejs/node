'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-metadata-' + process.pid);
let counter = 0;

function createMountedVfs() {
  const mp = mountPoint + '-' + (counter++);
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'hello');
  myVfs.symlinkSync('/file.txt', '/link.txt');
  myVfs.mount(mp);
  return { myVfs, mp };
}

// Test chmodSync changes the mode
{
  const { myVfs, mp } = createMountedVfs();
  const filePath = path.join(mp, 'file.txt');

  const before = fs.statSync(filePath);
  assert.strictEqual(before.mode & 0o777, 0o644);

  fs.chmodSync(filePath, 0o755);
  const after = fs.statSync(filePath);
  assert.strictEqual(after.mode & 0o777, 0o755);

  myVfs.unmount();
}

// Test chownSync changes uid/gid
{
  const { myVfs, mp } = createMountedVfs();
  const filePath = path.join(mp, 'file.txt');

  fs.chownSync(filePath, 1000, 2000);
  const stat = fs.statSync(filePath);
  assert.strictEqual(stat.uid, 1000);
  assert.strictEqual(stat.gid, 2000);

  myVfs.unmount();
}

// Test utimesSync updates atime and mtime
{
  const { myVfs, mp } = createMountedVfs();
  const filePath = path.join(mp, 'file.txt');

  const newAtime = new Date('2020-01-01T00:00:00Z');
  const newMtime = new Date('2020-06-15T12:00:00Z');

  fs.utimesSync(filePath, newAtime, newMtime);
  const stat = fs.statSync(filePath);

  // Allow 1 second tolerance for time conversion
  assert.ok(Math.abs(stat.atimeMs - newAtime.getTime()) < 1000);
  assert.ok(Math.abs(stat.mtimeMs - newMtime.getTime()) < 1000);

  myVfs.unmount();
}

// Test lutimesSync updates timestamps on symlink itself
{
  const { myVfs, mp } = createMountedVfs();
  const linkPath = path.join(mp, 'link.txt');

  const newAtime = new Date('2019-01-01T00:00:00Z');
  const newMtime = new Date('2019-06-15T12:00:00Z');

  fs.lutimesSync(linkPath, newAtime, newMtime);

  // Lstat should show the updated times on the symlink
  const linkStat = fs.lstatSync(linkPath);
  assert.ok(Math.abs(linkStat.mtimeMs - newMtime.getTime()) < 1000);

  myVfs.unmount();
}

// Test async chmod
{
  const { myVfs, mp } = createMountedVfs();
  const filePath = path.join(mp, 'file.txt');

  fs.chmod(filePath, 0o700, common.mustSucceed(() => {
    const stat = fs.statSync(filePath);
    assert.strictEqual(stat.mode & 0o777, 0o700);
    myVfs.unmount();
  }));
}

// Test promises chmod
{
  const { myVfs, mp } = createMountedVfs();
  const filePath = path.join(mp, 'file.txt');

  fs.promises.chmod(filePath, 0o600).then(common.mustCall(() => {
    const stat = fs.statSync(filePath);
    assert.strictEqual(stat.mode & 0o777, 0o600);
    myVfs.unmount();
  }));
}
