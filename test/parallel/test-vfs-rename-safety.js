'use strict';

// Tests for VFS renameSync safety:
// - Source preserved on destination validation failure
// - Cannot overwrite a directory with a file
// - Does not auto-create missing destination parents

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Source is preserved when destination parent is not a directory.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'src');
  myVfs.writeFileSync('/foo', 'notdir');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.renameSync('/mnt/src.txt', '/mnt/foo/bar'),
    { code: 'ENOTDIR' },
  );

  // Source must still exist
  assert.strictEqual(fs.existsSync('/mnt/src.txt'), true);

  myVfs.unmount();
}

// Cannot overwrite a directory with a file.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'src');
  myVfs.mkdirSync('/dir/sub', { recursive: true });
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.renameSync('/mnt/src.txt', '/mnt/dir'),
    { code: 'EISDIR' },
  );

  // Directory and sub should still exist
  assert.strictEqual(fs.statSync('/mnt/dir').isDirectory(), true);
  assert.strictEqual(fs.existsSync('/mnt/dir/sub'), true);

  myVfs.unmount();
}

// Does not auto-create missing destination parents.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'src');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.renameSync('/mnt/src.txt', '/mnt/nodir/dest.txt'),
    { code: 'ENOENT' },
  );

  // Source must still exist
  assert.strictEqual(fs.existsSync('/mnt/src.txt'), true);

  myVfs.unmount();
}
