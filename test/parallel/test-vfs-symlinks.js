'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// Test basic symlink creation
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/target.txt', 'Hello, World!');
  vfs.symlinkSync('/target.txt', '/link.txt');
  vfs.mount('/virtual');

  // Verify symlink exists
  assert.strictEqual(vfs.existsSync('/virtual/link.txt'), true);

  vfs.unmount();
}

// Test reading file through symlink
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/data', { recursive: true });
  vfs.writeFileSync('/data/file.txt', 'File content');
  vfs.symlinkSync('/data/file.txt', '/shortcut');
  vfs.mount('/virtual');

  const content = vfs.readFileSync('/virtual/shortcut', 'utf8');
  assert.strictEqual(content, 'File content');

  vfs.unmount();
}

// Test statSync follows symlinks (returns target's stats)
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/real.txt', 'x'.repeat(100));
  vfs.symlinkSync('/real.txt', '/link.txt');
  vfs.mount('/virtual');

  const statLink = vfs.statSync('/virtual/link.txt');
  const statReal = vfs.statSync('/virtual/real.txt');

  // Both should have the same size (the file's size)
  assert.strictEqual(statLink.size, 100);
  assert.strictEqual(statLink.size, statReal.size);

  // statSync should show it's a file, not a symlink
  assert.strictEqual(statLink.isFile(), true);
  assert.strictEqual(statLink.isSymbolicLink(), false);

  vfs.unmount();
}

// Test lstatSync does NOT follow symlinks
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/real.txt', 'x'.repeat(100));
  vfs.symlinkSync('/real.txt', '/link.txt');
  vfs.mount('/virtual');

  const lstat = vfs.lstatSync('/virtual/link.txt');

  // Lstat should show it's a symlink
  assert.strictEqual(lstat.isSymbolicLink(), true);
  assert.strictEqual(lstat.isFile(), false);

  // Size should be the length of the target path
  assert.strictEqual(lstat.size, '/real.txt'.length);

  vfs.unmount();
}

// Test readlinkSync returns symlink target
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/target.txt', 'content');
  vfs.symlinkSync('/target.txt', '/link.txt');
  vfs.mount('/virtual');

  const target = vfs.readlinkSync('/virtual/link.txt');
  assert.strictEqual(target, '/target.txt');

  vfs.unmount();
}

// Test readlinkSync throws EINVAL for non-symlinks
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/file.txt', 'content');
  vfs.mount('/virtual');

  assert.throws(() => {
    vfs.readlinkSync('/virtual/file.txt');
  }, { code: 'EINVAL' });

  vfs.unmount();
}

// Test symlink to directory
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/data', { recursive: true });
  vfs.writeFileSync('/data/file.txt', 'content');
  vfs.symlinkSync('/data', '/shortcut');
  vfs.mount('/virtual');

  // Reading through symlink directory
  const content = vfs.readFileSync('/virtual/shortcut/file.txt', 'utf8');
  assert.strictEqual(content, 'content');

  // Listing symlinked directory
  const files = vfs.readdirSync('/virtual/shortcut');
  assert.deepStrictEqual(files, ['file.txt']);

  vfs.unmount();
}

// Test relative symlinks
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/dir', { recursive: true });
  vfs.writeFileSync('/dir/file.txt', 'content');
  vfs.symlinkSync('file.txt', '/dir/link.txt'); // Relative target
  vfs.mount('/virtual');

  const content = vfs.readFileSync('/virtual/dir/link.txt', 'utf8');
  assert.strictEqual(content, 'content');

  // Readlink should return the relative target as-is
  const target = vfs.readlinkSync('/virtual/dir/link.txt');
  assert.strictEqual(target, 'file.txt');

  vfs.unmount();
}

// Test symlink chains (symlink pointing to another symlink)
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/file.txt', 'chained');
  vfs.symlinkSync('/file.txt', '/link1');
  vfs.symlinkSync('/link1', '/link2');
  vfs.symlinkSync('/link2', '/link3');
  vfs.mount('/virtual');

  // Should resolve through all symlinks
  const content = vfs.readFileSync('/virtual/link3', 'utf8');
  assert.strictEqual(content, 'chained');

  vfs.unmount();
}

// Test realpathSync resolves symlinks
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/actual', { recursive: true });
  vfs.writeFileSync('/actual/file.txt', 'content');
  vfs.symlinkSync('/actual', '/link');
  vfs.mount('/virtual');

  const realpath = vfs.realpathSync('/virtual/link/file.txt');
  assert.strictEqual(realpath, '/virtual/actual/file.txt');

  vfs.unmount();
}

// Test symlink loop detection (ELOOP)
{
  const vfs = fs.createVirtual();
  vfs.symlinkSync('/loop2', '/loop1');
  vfs.symlinkSync('/loop1', '/loop2');
  vfs.mount('/virtual');

  // statSync should throw ELOOP
  assert.throws(() => {
    vfs.statSync('/virtual/loop1');
  }, { code: 'ELOOP' });

  // realpathSync should throw ELOOP
  assert.throws(() => {
    vfs.realpathSync('/virtual/loop1');
  }, { code: 'ELOOP' });

  // lstatSync should still work (doesn't follow symlinks)
  const lstat = vfs.lstatSync('/virtual/loop1');
  assert.strictEqual(lstat.isSymbolicLink(), true);

  vfs.unmount();
}

// Test readdirSync with withFileTypes includes symlinks
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/dir/subdir', { recursive: true });
  vfs.writeFileSync('/dir/file.txt', 'content');
  vfs.symlinkSync('/dir/file.txt', '/dir/link');
  vfs.mount('/virtual');

  const entries = vfs.readdirSync('/virtual/dir', { withFileTypes: true });

  const file = entries.find((e) => e.name === 'file.txt');
  const subdir = entries.find((e) => e.name === 'subdir');
  const link = entries.find((e) => e.name === 'link');

  assert.strictEqual(file.isFile(), true);
  assert.strictEqual(subdir.isDirectory(), true);
  assert.strictEqual(link.isSymbolicLink(), true);

  vfs.unmount();
}

// Test async readlink
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/target', 'content');
  vfs.symlinkSync('/target', '/link');
  vfs.mount('/virtual');

  vfs.readlink('/virtual/link', common.mustSucceed((target) => {
    assert.strictEqual(target, '/target');
    vfs.unmount();
  }));
}

// Test async realpath with symlinks
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/real', { recursive: true });
  vfs.writeFileSync('/real/file.txt', 'content');
  vfs.symlinkSync('/real', '/link');
  vfs.mount('/virtual');

  vfs.realpath('/virtual/link/file.txt', common.mustSucceed((resolvedPath) => {
    assert.strictEqual(resolvedPath, '/virtual/real/file.txt');
    vfs.unmount();
  }));
}

// Test promises API - stat follows symlinks
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/file.txt', 'x'.repeat(50));
  vfs.symlinkSync('/file.txt', '/link.txt');
  vfs.mount('/virtual');

  (async () => {
    const stat = await vfs.promises.stat('/virtual/link.txt');
    assert.strictEqual(stat.isFile(), true);
    assert.strictEqual(stat.size, 50);
    vfs.unmount();
  })().then(common.mustCall());
}

// Test promises API - lstat does not follow symlinks
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/file.txt', 'x'.repeat(50));
  vfs.symlinkSync('/file.txt', '/link.txt');
  vfs.mount('/virtual');

  (async () => {
    const lstat = await vfs.promises.lstat('/virtual/link.txt');
    assert.strictEqual(lstat.isSymbolicLink(), true);
    vfs.unmount();
  })().then(common.mustCall());
}

// Test promises API - readlink
{
  const vfs = fs.createVirtual();
  vfs.writeFileSync('/target', 'content');
  vfs.symlinkSync('/target', '/link');
  vfs.mount('/virtual');

  (async () => {
    const target = await vfs.promises.readlink('/virtual/link');
    assert.strictEqual(target, '/target');
    vfs.unmount();
  })().then(common.mustCall());
}

// Test promises API - realpath resolves symlinks
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/real', { recursive: true });
  vfs.writeFileSync('/real/file.txt', 'content');
  vfs.symlinkSync('/real', '/link');
  vfs.mount('/virtual');

  (async () => {
    const resolved = await vfs.promises.realpath('/virtual/link/file.txt');
    assert.strictEqual(resolved, '/virtual/real/file.txt');
    vfs.unmount();
  })().then(common.mustCall());
}

// Test broken symlink (target doesn't exist)
{
  const vfs = fs.createVirtual();
  vfs.symlinkSync('/nonexistent', '/broken');
  vfs.mount('/virtual');

  // statSync should throw ENOENT for broken symlink
  assert.throws(() => {
    vfs.statSync('/virtual/broken');
  }, { code: 'ENOENT' });

  // lstatSync should work (the symlink itself exists)
  const lstat = vfs.lstatSync('/virtual/broken');
  assert.strictEqual(lstat.isSymbolicLink(), true);

  // readlinkSync should work (returns target path)
  const target = vfs.readlinkSync('/virtual/broken');
  assert.strictEqual(target, '/nonexistent');

  vfs.unmount();
}

// Test symlink with parent traversal (..)
{
  const vfs = fs.createVirtual();
  vfs.mkdirSync('/a', { recursive: true });
  vfs.mkdirSync('/b', { recursive: true });
  vfs.writeFileSync('/a/file.txt', 'content');
  vfs.symlinkSync('../a/file.txt', '/b/link');
  vfs.mount('/virtual');

  const content = vfs.readFileSync('/virtual/b/link', 'utf8');
  assert.strictEqual(content, 'content');

  vfs.unmount();
}

console.log('All VFS symlink tests passed!');
