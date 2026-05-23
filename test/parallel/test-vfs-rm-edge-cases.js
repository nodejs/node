// Flags: --experimental-vfs
'use strict';

// Tests for VFS rmSync edge cases:
// - rmSync on a directory without recursive must throw EISDIR
// - rmSync on a symlink must not recurse into the target directory

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// rmSync on a directory without { recursive: true } must throw EISDIR.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');

  assert.throws(() => myVfs.rmSync('/dir'), { code: 'EISDIR' });
  // Directory should still exist after the failed rmSync
  assert.strictEqual(myVfs.existsSync('/dir'), true);
}

// rmSync(link, { recursive: true }) removes symlink without recursing
// into the target directory.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir/sub', { recursive: true });
  myVfs.writeFileSync('/dir/sub/file.txt', 'x');
  myVfs.symlinkSync('/dir', '/link');

  myVfs.rmSync('/link', { recursive: true });

  // Symlink should be removed
  assert.strictEqual(myVfs.existsSync('/link'), false);
  // Target directory and its contents should still exist
  assert.strictEqual(myVfs.existsSync('/dir/sub/file.txt'), true);
}

// rmSync with force: true ignores ENOENT
{
  const myVfs = vfs.create();
  myVfs.rmSync('/missing.txt', { force: true });
}

// rmSync without force on missing path throws ENOENT
{
  const myVfs = vfs.create();
  assert.throws(() => myVfs.rmSync('/missing.txt'), { code: 'ENOENT' });
}

// promises.rm equivalents
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d/sub', { recursive: true });
  myVfs.writeFileSync('/d/sub/f.txt', 'x');

  await assert.rejects(myVfs.promises.rm('/d'), { code: 'EISDIR' });
  await myVfs.promises.rm('/d', { recursive: true });
  assert.strictEqual(myVfs.existsSync('/d'), false);

  await myVfs.promises.rm('/missing', { force: true });
  await assert.rejects(myVfs.promises.rm('/missing'), { code: 'ENOENT' });

  // promises.rm on symlink unlinks without recursion
  myVfs.mkdirSync('/d2/sub', { recursive: true });
  myVfs.writeFileSync('/d2/sub/file.txt', 'x');
  myVfs.symlinkSync('/d2', '/link2');
  await myVfs.promises.rm('/link2', { recursive: true });
  assert.strictEqual(myVfs.existsSync('/link2'), false);
  assert.strictEqual(myVfs.existsSync('/d2/sub/file.txt'), true);
})().then(common.mustCall());
