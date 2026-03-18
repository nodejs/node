'use strict';

// Tests for VFS symlink edge cases:
// - lstatSync/readlinkSync on broken symlinks
// - Intermediate symlink segment resolution
// - symlinkSync does not auto-create missing parents

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Broken symlinks: lstatSync and readlinkSync must work even when the
// symlink target does not exist.
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/no-such-target', '/link');
  myVfs.mount('/mnt');

  const stats = fs.lstatSync('/mnt/link');
  assert.strictEqual(stats.isSymbolicLink(), true);

  assert.strictEqual(fs.readlinkSync('/mnt/link'), '/no-such-target');

  myVfs.unmount();
}

// Intermediate symlink segments are followed for lstat/unlink.
// A symlinked intermediate directory component should be resolved.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real', { recursive: true });
  myVfs.writeFileSync('/real/file.txt', 'x');
  myVfs.symlinkSync('/real', '/linkdir');
  myVfs.mount('/mnt');

  // Lstat through a symlinked directory
  const stats = fs.lstatSync('/mnt/linkdir/file.txt');
  assert.strictEqual(stats.isFile(), true);

  // Unlink through a symlinked directory
  fs.unlinkSync('/mnt/linkdir/file.txt');
  assert.strictEqual(fs.existsSync('/mnt/linkdir/file.txt'), false);

  myVfs.unmount();
}

// symlinkSync does not auto-create missing parent directories.
// Creating a symlink whose parent path does not exist must throw ENOENT.
{
  const myVfs = vfs.create();
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.symlinkSync('/target', '/mnt/missing/dir/link'),
    { code: 'ENOENT' },
  );

  myVfs.unmount();
}
