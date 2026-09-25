// Flags: --experimental-vfs
'use strict';

// Symlink behaviour in the default (memory) VFS:
// - Loop detection (ELOOP)
// - Absolute and relative targets
// - Symlinked parent directories transparently follow

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Direct symlink loop: /a -> /b -> /a
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/b', '/a');
  myVfs.symlinkSync('/a', '/b');
  assert.throws(() => myVfs.statSync('/a'), { code: 'ELOOP' });
  assert.throws(() => myVfs.realpathSync('/a'), { code: 'ELOOP' });
}

// Symlink loop in an intermediate path component
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/loop2', '/loop1');
  myVfs.symlinkSync('/loop1', '/loop2');
  assert.throws(() => myVfs.statSync('/loop1/sub'), { code: 'ELOOP' });
}

// Absolute symlink target inside the VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.writeFileSync('/dir/file.txt', 'hi');
  myVfs.symlinkSync('/dir', '/abs-link');
  assert.strictEqual(myVfs.readFileSync('/abs-link/file.txt', 'utf8'), 'hi');
}

// Symlinked parent directory transparently follows on read+write
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real-dir');
  myVfs.writeFileSync('/real-dir/file.txt', 'hello');
  myVfs.symlinkSync('/real-dir', '/link-dir');

  assert.strictEqual(myVfs.readFileSync('/link-dir/file.txt', 'utf8'), 'hello');
  myVfs.writeFileSync('/link-dir/new.txt', 'new');
  assert.strictEqual(myVfs.readFileSync('/real-dir/new.txt', 'utf8'), 'new');
}

// Symlink onto an existing path throws EEXIST
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'x');
  assert.throws(() => myVfs.symlinkSync('/a.txt', '/a.txt'), { code: 'EEXIST' });
}
