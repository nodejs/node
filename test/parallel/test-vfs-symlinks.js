'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test basic symlink creation
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/target.txt', 'Hello, World!');
  myVfs.symlinkSync('/target.txt', '/link.txt');
  myVfs.mount('/virtual');

  // Verify symlink exists
  assert.strictEqual(myVfs.existsSync('/virtual/link.txt'), true);

  myVfs.unmount();
}

// Test reading file through symlink
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/data', { recursive: true });
  myVfs.writeFileSync('/data/file.txt', 'File content');
  myVfs.symlinkSync('/data/file.txt', '/shortcut');
  myVfs.mount('/virtual');

  const content = myVfs.readFileSync('/virtual/shortcut', 'utf8');
  assert.strictEqual(content, 'File content');

  myVfs.unmount();
}

// Test statSync follows symlinks (returns target's stats)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/real.txt', 'x'.repeat(100));
  myVfs.symlinkSync('/real.txt', '/link.txt');
  myVfs.mount('/virtual');

  const statLink = myVfs.statSync('/virtual/link.txt');
  const statReal = myVfs.statSync('/virtual/real.txt');

  // Both should have the same size (the file's size)
  assert.strictEqual(statLink.size, 100);
  assert.strictEqual(statLink.size, statReal.size);

  // statSync should show it's a file, not a symlink
  assert.strictEqual(statLink.isFile(), true);
  assert.strictEqual(statLink.isSymbolicLink(), false);

  myVfs.unmount();
}

// Test lstatSync does NOT follow symlinks
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/real.txt', 'x'.repeat(100));
  myVfs.symlinkSync('/real.txt', '/link.txt');
  myVfs.mount('/virtual');

  const lstat = myVfs.lstatSync('/virtual/link.txt');

  // Lstat should show it's a symlink
  assert.strictEqual(lstat.isSymbolicLink(), true);
  assert.strictEqual(lstat.isFile(), false);

  // Size should be the length of the target path
  assert.strictEqual(lstat.size, '/real.txt'.length);

  myVfs.unmount();
}

// Test readlinkSync returns symlink target
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/target.txt', 'content');
  myVfs.symlinkSync('/target.txt', '/link.txt');
  myVfs.mount('/virtual');

  const target = myVfs.readlinkSync('/virtual/link.txt');
  assert.strictEqual(target, '/target.txt');

  myVfs.unmount();
}

// Test readlinkSync throws EINVAL for non-symlinks
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'content');
  myVfs.mount('/virtual');

  assert.throws(() => {
    myVfs.readlinkSync('/virtual/file.txt');
  }, { code: 'EINVAL' });

  myVfs.unmount();
}

// Test symlink to directory
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/data', { recursive: true });
  myVfs.writeFileSync('/data/file.txt', 'content');
  myVfs.symlinkSync('/data', '/shortcut');
  myVfs.mount('/virtual');

  // Reading through symlink directory
  const content = myVfs.readFileSync('/virtual/shortcut/file.txt', 'utf8');
  assert.strictEqual(content, 'content');

  // Listing symlinked directory
  const files = myVfs.readdirSync('/virtual/shortcut');
  assert.deepStrictEqual(files, ['file.txt']);

  myVfs.unmount();
}

// Test relative symlinks
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir', { recursive: true });
  myVfs.writeFileSync('/dir/file.txt', 'content');
  myVfs.symlinkSync('file.txt', '/dir/link.txt'); // Relative target
  myVfs.mount('/virtual');

  const content = myVfs.readFileSync('/virtual/dir/link.txt', 'utf8');
  assert.strictEqual(content, 'content');

  // Readlink should return the relative target as-is
  const target = myVfs.readlinkSync('/virtual/dir/link.txt');
  assert.strictEqual(target, 'file.txt');

  myVfs.unmount();
}

// Test symlink chains (symlink pointing to another symlink)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'chained');
  myVfs.symlinkSync('/file.txt', '/link1');
  myVfs.symlinkSync('/link1', '/link2');
  myVfs.symlinkSync('/link2', '/link3');
  myVfs.mount('/virtual');

  // Should resolve through all symlinks
  const content = myVfs.readFileSync('/virtual/link3', 'utf8');
  assert.strictEqual(content, 'chained');

  myVfs.unmount();
}

// Test realpathSync resolves symlinks
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/actual', { recursive: true });
  myVfs.writeFileSync('/actual/file.txt', 'content');
  myVfs.symlinkSync('/actual', '/link');
  myVfs.mount('/virtual');

  const realpath = myVfs.realpathSync('/virtual/link/file.txt');
  assert.strictEqual(realpath, '/virtual/actual/file.txt');

  myVfs.unmount();
}

// Test symlink loop detection (ELOOP)
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/loop2', '/loop1');
  myVfs.symlinkSync('/loop1', '/loop2');
  myVfs.mount('/virtual');

  // statSync should throw ELOOP
  assert.throws(() => {
    myVfs.statSync('/virtual/loop1');
  }, { code: 'ELOOP' });

  // realpathSync should throw ELOOP
  assert.throws(() => {
    myVfs.realpathSync('/virtual/loop1');
  }, { code: 'ELOOP' });

  // lstatSync should still work (doesn't follow symlinks)
  const lstat = myVfs.lstatSync('/virtual/loop1');
  assert.strictEqual(lstat.isSymbolicLink(), true);

  myVfs.unmount();
}

// Test readdirSync with withFileTypes includes symlinks
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir/subdir', { recursive: true });
  myVfs.writeFileSync('/dir/file.txt', 'content');
  myVfs.symlinkSync('/dir/file.txt', '/dir/link');
  myVfs.mount('/virtual');

  const entries = myVfs.readdirSync('/virtual/dir', { withFileTypes: true });

  const file = entries.find((e) => e.name === 'file.txt');
  const subdir = entries.find((e) => e.name === 'subdir');
  const link = entries.find((e) => e.name === 'link');

  assert.strictEqual(file.isFile(), true);
  assert.strictEqual(subdir.isDirectory(), true);
  assert.strictEqual(link.isSymbolicLink(), true);

  myVfs.unmount();
}

// Test async readlink
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/target', 'content');
  myVfs.symlinkSync('/target', '/link');
  myVfs.mount('/virtual');

  myVfs.readlink('/virtual/link', common.mustSucceed((target) => {
    assert.strictEqual(target, '/target');
    myVfs.unmount();
  }));
}

// Test async realpath with symlinks
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real', { recursive: true });
  myVfs.writeFileSync('/real/file.txt', 'content');
  myVfs.symlinkSync('/real', '/link');
  myVfs.mount('/virtual');

  myVfs.realpath('/virtual/link/file.txt', common.mustSucceed((resolvedPath) => {
    assert.strictEqual(resolvedPath, '/virtual/real/file.txt');
    myVfs.unmount();
  }));
}

// Test promises API - stat follows symlinks
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x'.repeat(50));
  myVfs.symlinkSync('/file.txt', '/link.txt');
  myVfs.mount('/virtual');

  (async () => {
    const stat = await myVfs.promises.stat('/virtual/link.txt');
    assert.strictEqual(stat.isFile(), true);
    assert.strictEqual(stat.size, 50);
    myVfs.unmount();
  })().then(common.mustCall());
}

// Test promises API - lstat does not follow symlinks
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x'.repeat(50));
  myVfs.symlinkSync('/file.txt', '/link.txt');
  myVfs.mount('/virtual');

  (async () => {
    const lstat = await myVfs.promises.lstat('/virtual/link.txt');
    assert.strictEqual(lstat.isSymbolicLink(), true);
    myVfs.unmount();
  })().then(common.mustCall());
}

// Test promises API - readlink
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/target', 'content');
  myVfs.symlinkSync('/target', '/link');
  myVfs.mount('/virtual');

  (async () => {
    const target = await myVfs.promises.readlink('/virtual/link');
    assert.strictEqual(target, '/target');
    myVfs.unmount();
  })().then(common.mustCall());
}

// Test promises API - realpath resolves symlinks
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real', { recursive: true });
  myVfs.writeFileSync('/real/file.txt', 'content');
  myVfs.symlinkSync('/real', '/link');
  myVfs.mount('/virtual');

  (async () => {
    const resolved = await myVfs.promises.realpath('/virtual/link/file.txt');
    assert.strictEqual(resolved, '/virtual/real/file.txt');
    myVfs.unmount();
  })().then(common.mustCall());
}

// Test broken symlink (target doesn't exist)
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/nonexistent', '/broken');
  myVfs.mount('/virtual');

  // statSync should throw ENOENT for broken symlink
  assert.throws(() => {
    myVfs.statSync('/virtual/broken');
  }, { code: 'ENOENT' });

  // lstatSync should work (the symlink itself exists)
  const lstat = myVfs.lstatSync('/virtual/broken');
  assert.strictEqual(lstat.isSymbolicLink(), true);

  // readlinkSync should work (returns target path)
  const target = myVfs.readlinkSync('/virtual/broken');
  assert.strictEqual(target, '/nonexistent');

  myVfs.unmount();
}

// Test symlink target created after symlink (dangling symlink becomes valid)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');

  // Create symlink pointing to non-existent target
  myVfs.symlinkSync('/dir/target.txt', '/dir/link');
  myVfs.mount('/virtual');

  // Initially the symlink is broken
  assert.throws(() => {
    myVfs.statSync('/virtual/dir/link');
  }, { code: 'ENOENT' });

  // lstatSync works (symlink itself exists)
  assert.strictEqual(myVfs.lstatSync('/virtual/dir/link').isSymbolicLink(), true);

  // Now create the target file
  myVfs.writeFileSync('/virtual/dir/target.txt', 'created after symlink');

  // Now the symlink should work
  const content = myVfs.readFileSync('/virtual/dir/link', 'utf8');
  assert.strictEqual(content, 'created after symlink');

  // Stat should also work now
  const stats = myVfs.statSync('/virtual/dir/link');
  assert.strictEqual(stats.isFile(), true);

  myVfs.unmount();
}

// Test symlink with parent traversal (..)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a', { recursive: true });
  myVfs.mkdirSync('/b', { recursive: true });
  myVfs.writeFileSync('/a/file.txt', 'content');
  myVfs.symlinkSync('../a/file.txt', '/b/link');
  myVfs.mount('/virtual');

  const content = myVfs.readFileSync('/virtual/b/link', 'utf8');
  assert.strictEqual(content, 'content');

  myVfs.unmount();
}
