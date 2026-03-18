'use strict';

// Tests for VFS rmSync edge cases:
// - rmSync on a directory without recursive must throw EISDIR
// - rmSync on a symlink must not recurse into the target directory

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// rmSync on a directory without { recursive: true } must throw EISDIR.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.mount('/mnt');

  assert.throws(() => fs.rmSync('/mnt/dir'), { code: 'EISDIR' });
  // Directory should still exist after the failed rmSync
  assert.strictEqual(fs.existsSync('/mnt/dir'), true);

  myVfs.unmount();
}

// rmSync(link, { recursive: true }) removes symlink without recursing
// into the target directory.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir/sub', { recursive: true });
  myVfs.writeFileSync('/dir/sub/file.txt', 'x');
  myVfs.symlinkSync('/dir', '/link');
  myVfs.mount('/mnt');

  fs.rmSync('/mnt/link', { recursive: true });

  // Symlink should be removed
  assert.strictEqual(fs.existsSync('/mnt/link'), false);
  // Target directory and its contents should still exist
  assert.strictEqual(fs.existsSync('/mnt/dir/sub/file.txt'), true);

  myVfs.unmount();
}
