'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Destructure fs methods BEFORE mounting any VFS.
// This is the key scenario: with monkey-patching, these captured references
// would bypass VFS. With the new in-function guards, they work correctly.
const {
  readFileSync,
  existsSync,
  statSync,
  lstatSync,
  readdirSync,
  realpathSync,
} = require('fs');

const myVfs = vfs.create();
myVfs.mkdirSync('/sub', { recursive: true });
myVfs.writeFileSync('/file.txt', 'hello from vfs');
myVfs.writeFileSync('/sub/nested.txt', 'nested content');
myVfs.mount('/vfs_destr');

// readFileSync works through destructured reference
{
  const content = readFileSync('/vfs_destr/file.txt', 'utf8');
  assert.strictEqual(content, 'hello from vfs');
}

// existsSync works through destructured reference
{
  assert.strictEqual(existsSync('/vfs_destr/file.txt'), true);
  assert.strictEqual(existsSync('/vfs_destr/nonexistent'), false);
}

// statSync works through destructured reference
{
  const stats = statSync('/vfs_destr/file.txt');
  assert.strictEqual(stats.isFile(), true);
  assert.strictEqual(stats.isDirectory(), false);
}

// lstatSync works through destructured reference
{
  const stats = lstatSync('/vfs_destr/file.txt');
  assert.strictEqual(stats.isFile(), true);
}

// readdirSync works through destructured reference
{
  const entries = readdirSync('/vfs_destr');
  assert.ok(entries.includes('file.txt'));
  assert.ok(entries.includes('sub'));
}

// realpathSync works through destructured reference
{
  const real = realpathSync('/vfs_destr/file.txt');
  assert.strictEqual(real, '/vfs_destr/file.txt');
}

// Also test fs/promises destructuring
const { readdir, lstat } = require('fs/promises');

async function testPromises() {
  const entries = await readdir('/vfs_destr');
  assert.ok(entries.includes('file.txt'));

  const stats = await lstat('/vfs_destr/file.txt');
  assert.strictEqual(stats.isFile(), true);
}

testPromises().then(common.mustCall(() => {
  myVfs.unmount();
}));
