'use strict';

// Tests for VFS copyFile mode support:
// - COPYFILE_EXCL throws when destination exists
// - Without COPYFILE_EXCL, copy overwrites destination

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// copyFileSync with COPYFILE_EXCL throws when destination exists.
{
  const { COPYFILE_EXCL } = fs.constants;
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'src');
  myVfs.writeFileSync('/dst.txt', 'dst');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.copyFileSync('/mnt/src.txt', '/mnt/dst.txt', COPYFILE_EXCL),
    { code: 'EEXIST' },
  );

  // Destination content should be unchanged
  assert.strictEqual(fs.readFileSync('/mnt/dst.txt', 'utf8'), 'dst');

  myVfs.unmount();
}

// copyFileSync without COPYFILE_EXCL succeeds and overwrites.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'new-content');
  myVfs.writeFileSync('/dst.txt', 'old');
  myVfs.mount('/mnt');

  fs.copyFileSync('/mnt/src.txt', '/mnt/dst.txt');
  assert.strictEqual(fs.readFileSync('/mnt/dst.txt', 'utf8'), 'new-content');

  myVfs.unmount();
}
