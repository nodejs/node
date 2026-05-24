// Flags: --experimental-vfs
'use strict';

// Verify { bigint: true } returns BigInt values for VFS stats.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');

const st = myVfs.statSync('/file.txt', { bigint: true });
assert.strictEqual(typeof st.size, 'bigint');
assert.strictEqual(st.size, 1n);
assert.strictEqual(typeof st.ino, 'bigint');
assert.strictEqual(typeof st.mode, 'bigint');

// Bigint stats for directories
{
  const v = vfs.create();
  v.mkdirSync('/dir');
  const st = v.statSync('/dir', { bigint: true });
  assert.strictEqual(typeof st.size, 'bigint');
  assert.strictEqual(st.isDirectory(), true);
}

// Bigint stats for symlinks via lstat
{
  const v = vfs.create();
  v.mkdirSync('/dir');
  v.symlinkSync('/dir', '/link');
  const st = v.lstatSync('/link', { bigint: true });
  assert.strictEqual(typeof st.size, 'bigint');
  assert.strictEqual(st.isSymbolicLink(), true);
}
