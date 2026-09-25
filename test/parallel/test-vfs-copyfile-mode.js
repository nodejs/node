// Flags: --experimental-vfs
'use strict';

// Tests for VFS copyFile mode support:
// - COPYFILE_EXCL throws when destination exists
// - Without COPYFILE_EXCL, copy overwrites destination

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const { COPYFILE_EXCL } = fs.constants;

// copyFileSync with COPYFILE_EXCL throws when destination exists.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'src');
  myVfs.writeFileSync('/dst.txt', 'dst');

  assert.throws(
    () => myVfs.copyFileSync('/src.txt', '/dst.txt', COPYFILE_EXCL),
    { code: 'EEXIST' },
  );

  assert.strictEqual(myVfs.readFileSync('/dst.txt', 'utf8'), 'dst');
}

// copyFileSync without COPYFILE_EXCL succeeds and overwrites.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'new-content');
  myVfs.writeFileSync('/dst.txt', 'old');

  myVfs.copyFileSync('/src.txt', '/dst.txt');
  assert.strictEqual(myVfs.readFileSync('/dst.txt', 'utf8'), 'new-content');
}

// promises.copyFile with COPYFILE_EXCL
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 's');
  myVfs.writeFileSync('/dst.txt', 'd');

  await assert.rejects(
    myVfs.promises.copyFile('/src.txt', '/dst.txt', COPYFILE_EXCL),
    { code: 'EEXIST' },
  );

  await myVfs.promises.copyFile('/src.txt', '/dst.txt');
  assert.strictEqual(myVfs.readFileSync('/dst.txt', 'utf8'), 's');
})().then(common.mustCall());
