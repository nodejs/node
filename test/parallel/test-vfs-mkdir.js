// Flags: --experimental-vfs
'use strict';

// mkdirSync / rmdirSync behaviour: return value, recursive option, mode
// option, error cases.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// mkdirSync({ recursive: true }) returns the path of the first newly-
// created directory when some parents already exist.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a');
  const result = myVfs.mkdirSync('/a/b/c', { recursive: true });
  assert.strictEqual(result, '/a/b');
}

// Recursive mkdir follows symlinked intermediate directories, but returns the
// path of the first created directory as requested by the caller.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/target');
  myVfs.symlinkSync('/target', '/link');

  const result = myVfs.mkdirSync('/link/subdir/deep', { recursive: true });

  assert.strictEqual(result, '/link/subdir');
  assert.strictEqual(myVfs.existsSync('/target/subdir/deep'), true);
  assert.strictEqual(myVfs.existsSync('/link/subdir/deep'), true);
}

// Recursive mkdir also resolves relative symlink targets from the symlink's
// resolved parent directory.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/parent/target', { recursive: true });
  myVfs.symlinkSync('target', '/parent/link');

  myVfs.mkdirSync('/parent/link/subdir', { recursive: true });

  assert.strictEqual(myVfs.existsSync('/parent/target/subdir'), true);
}

// Recursive mkdir through symlinks keeps native error behavior for bad
// intermediate targets.
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/missing', '/dangling');
  assert.throws(
    () => myVfs.mkdirSync('/dangling/subdir', { recursive: true }),
    { code: 'ENOENT' });
}

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file', 'x');
  myVfs.symlinkSync('/file', '/link');
  assert.throws(
    () => myVfs.mkdirSync('/link/subdir', { recursive: true }),
    { code: 'ENOTDIR' });
}

// mkdirSync with explicit mode (non-recursive)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d-mode', { mode: 0o700 });
  assert.strictEqual(myVfs.statSync('/d-mode').mode & 0o777, 0o700);
}

// mkdirSync with explicit mode + recursive
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/r-mode/sub/deep', { recursive: true, mode: 0o700 });
  assert.strictEqual(myVfs.statSync('/r-mode/sub/deep').mode & 0o777, 0o700);
}

// Recursive mkdir through a regular-file blocker throws ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/blocker', 'x');
  assert.throws(
    () => myVfs.mkdirSync('/blocker/sub', { recursive: true }),
    { code: 'ENOTDIR' });
}

// Rmdir on a non-empty directory throws ENOTEMPTY
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/x', '');
  assert.throws(() => myVfs.rmdirSync('/d'), { code: 'ENOTEMPTY' });
}

// promises.mkdir uses the same recursive symlink handling.
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/target');
  myVfs.symlinkSync('/target', '/link');

  const result = await myVfs.promises.mkdir('/link/subdir/deep',
                                            { recursive: true });

  assert.strictEqual(result, '/link/subdir');
  assert.strictEqual(myVfs.existsSync('/target/subdir/deep'), true);
})().then(common.mustCall());
