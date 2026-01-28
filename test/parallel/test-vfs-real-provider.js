'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();

const testDir = path.join(tmpdir.path, 'vfs-real-provider');
fs.mkdirSync(testDir, { recursive: true });

// Test basic RealFSProvider creation
{
  const provider = new vfs.RealFSProvider(testDir);
  assert.ok(provider);
  assert.strictEqual(provider.rootPath, testDir);
  assert.strictEqual(provider.readonly, false);
  assert.strictEqual(provider.supportsSymlinks, true);
}

// Test invalid rootPath
{
  assert.throws(() => {
    new vfs.RealFSProvider('');
  }, { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => {
    new vfs.RealFSProvider(123);
  }, { code: 'ERR_INVALID_ARG_VALUE' });
}

// Test creating VFS with RealFSProvider
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  assert.ok(realVfs);
  assert.strictEqual(realVfs.readonly, false);
}

// Test reading and writing files through RealFSProvider
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  // Write a file through VFS
  realVfs.writeFileSync('/hello.txt', 'Hello from VFS!');

  // Verify it exists on the real file system
  const realPath = path.join(testDir, 'hello.txt');
  assert.strictEqual(fs.existsSync(realPath), true);
  assert.strictEqual(fs.readFileSync(realPath, 'utf8'), 'Hello from VFS!');

  // Read it back through VFS
  assert.strictEqual(realVfs.readFileSync('/hello.txt', 'utf8'), 'Hello from VFS!');

  // Clean up
  fs.unlinkSync(realPath);
}

// Test stat operations
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  // Create a file and directory
  fs.writeFileSync(path.join(testDir, 'stat-test.txt'), 'content');
  fs.mkdirSync(path.join(testDir, 'stat-dir'), { recursive: true });

  const fileStat = realVfs.statSync('/stat-test.txt');
  assert.strictEqual(fileStat.isFile(), true);
  assert.strictEqual(fileStat.isDirectory(), false);

  const dirStat = realVfs.statSync('/stat-dir');
  assert.strictEqual(dirStat.isFile(), false);
  assert.strictEqual(dirStat.isDirectory(), true);

  // Test ENOENT
  assert.throws(() => {
    realVfs.statSync('/nonexistent');
  }, { code: 'ENOENT' });

  // Clean up
  fs.unlinkSync(path.join(testDir, 'stat-test.txt'));
  fs.rmdirSync(path.join(testDir, 'stat-dir'));
}

// Test readdirSync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.mkdirSync(path.join(testDir, 'readdir-test', 'subdir'), { recursive: true });
  fs.writeFileSync(path.join(testDir, 'readdir-test', 'a.txt'), 'a');
  fs.writeFileSync(path.join(testDir, 'readdir-test', 'b.txt'), 'b');

  const entries = realVfs.readdirSync('/readdir-test');
  assert.deepStrictEqual(entries.sort(), ['a.txt', 'b.txt', 'subdir']);

  // With file types
  const dirents = realVfs.readdirSync('/readdir-test', { withFileTypes: true });
  assert.strictEqual(dirents.length, 3);

  const fileEntry = dirents.find((d) => d.name === 'a.txt');
  assert.ok(fileEntry);
  assert.strictEqual(fileEntry.isFile(), true);

  const dirEntry = dirents.find((d) => d.name === 'subdir');
  assert.ok(dirEntry);
  assert.strictEqual(dirEntry.isDirectory(), true);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'readdir-test', 'a.txt'));
  fs.unlinkSync(path.join(testDir, 'readdir-test', 'b.txt'));
  fs.rmdirSync(path.join(testDir, 'readdir-test', 'subdir'));
  fs.rmdirSync(path.join(testDir, 'readdir-test'));
}

// Test mkdir and rmdir
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  realVfs.mkdirSync('/new-dir');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'new-dir')), true);
  assert.strictEqual(fs.statSync(path.join(testDir, 'new-dir')).isDirectory(), true);

  realVfs.rmdirSync('/new-dir');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'new-dir')), false);
}

// Test unlink
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'to-delete.txt'), 'delete me');
  assert.strictEqual(realVfs.existsSync('/to-delete.txt'), true);

  realVfs.unlinkSync('/to-delete.txt');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'to-delete.txt')), false);
}

// Test rename
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'old-name.txt'), 'rename me');
  realVfs.renameSync('/old-name.txt', '/new-name.txt');

  assert.strictEqual(fs.existsSync(path.join(testDir, 'old-name.txt')), false);
  assert.strictEqual(fs.existsSync(path.join(testDir, 'new-name.txt')), true);
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'new-name.txt'), 'utf8'), 'rename me');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'new-name.txt'));
}

// Test path traversal prevention
{
  const subDir = path.join(testDir, 'sandbox');
  fs.mkdirSync(subDir, { recursive: true });

  const realVfs = vfs.create(new vfs.RealFSProvider(subDir));

  // Trying to access parent via .. should fail
  assert.throws(() => {
    realVfs.statSync('/../hello.txt');
  }, { code: 'ENOENT' });

  assert.throws(() => {
    realVfs.readFileSync('/../../../etc/passwd');
  }, { code: 'ENOENT' });

  // Clean up
  fs.rmdirSync(subDir);
}

// Test mounting RealFSProvider
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'mounted.txt'), 'mounted content');

  realVfs.mount('/virtual');

  // Now should be able to read through standard fs
  const content = fs.readFileSync('/virtual/mounted.txt', 'utf8');
  assert.strictEqual(content, 'mounted content');

  realVfs.unmount();

  // Clean up
  fs.unlinkSync(path.join(testDir, 'mounted.txt'));
}

// Test async operations
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  await realVfs.promises.writeFile('/async-test.txt', 'async content');
  const content = await realVfs.promises.readFile('/async-test.txt', 'utf8');
  assert.strictEqual(content, 'async content');

  const stat = await realVfs.promises.stat('/async-test.txt');
  assert.strictEqual(stat.isFile(), true);

  await realVfs.promises.unlink('/async-test.txt');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-test.txt')), false);
})().then(common.mustCall());

// Test copyFile
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'source.txt'), 'copy me');
  realVfs.copyFileSync('/source.txt', '/dest.txt');

  assert.strictEqual(fs.existsSync(path.join(testDir, 'dest.txt')), true);
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'dest.txt'), 'utf8'), 'copy me');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'source.txt'));
  fs.unlinkSync(path.join(testDir, 'dest.txt'));
}

// Test realpathSync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'real.txt'), 'content');

  const resolved = realVfs.realpathSync('/real.txt');
  assert.strictEqual(resolved, '/real.txt');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'real.txt'));
}
