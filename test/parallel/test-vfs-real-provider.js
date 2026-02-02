// Flags: --expose-internals
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

// Test file handle operations via openSync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'handle-test.txt'), 'hello world');

  const fd = realVfs.openSync('/handle-test.txt', 'r');
  assert.ok(fd >= 10000);
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Read via file handle
  const buffer = Buffer.alloc(5);
  const bytesRead = handle.entry.readSync(buffer, 0, 5, 0);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buffer.toString(), 'hello');

  realVfs.closeSync(fd);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'handle-test.txt'));
}

// Test file handle write operations
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  const fd = realVfs.openSync('/write-handle.txt', 'w');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const buffer = Buffer.from('written via handle');
  const bytesWritten = handle.entry.writeSync(buffer, 0, buffer.length, 0);
  assert.strictEqual(bytesWritten, buffer.length);

  realVfs.closeSync(fd);

  // Verify content
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'write-handle.txt'), 'utf8'), 'written via handle');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'write-handle.txt'));
}

// Test async file handle read
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'async-handle.txt'), 'async read test');

  const fd = realVfs.openSync('/async-handle.txt', 'r');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const buffer = Buffer.alloc(10);
  const result = await handle.entry.read(buffer, 0, 10, 0);
  assert.strictEqual(result.bytesRead, 10);
  assert.strictEqual(buffer.toString(), 'async read');

  realVfs.closeSync(fd);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-handle.txt'));
})().then(common.mustCall());

// Test async file handle write
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  const fd = realVfs.openSync('/async-write.txt', 'w');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const buffer = Buffer.from('async write');
  const result = await handle.entry.write(buffer, 0, buffer.length, 0);
  assert.strictEqual(result.bytesWritten, buffer.length);

  realVfs.closeSync(fd);

  // Verify content
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'async-write.txt'), 'utf8'), 'async write');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-write.txt'));
})().then(common.mustCall());

// Test async file handle stat
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'stat-handle.txt'), 'stat test');

  const fd = realVfs.openSync('/stat-handle.txt', 'r');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const stat = await handle.entry.stat();
  assert.strictEqual(stat.isFile(), true);

  realVfs.closeSync(fd);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'stat-handle.txt'));
})().then(common.mustCall());

// Test async file handle truncate
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'truncate-handle.txt'), 'truncate this');

  const fd = realVfs.openSync('/truncate-handle.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  await handle.entry.truncate(8);
  realVfs.closeSync(fd);

  // Verify content was truncated
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'truncate-handle.txt'), 'utf8'), 'truncate');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'truncate-handle.txt'));
})().then(common.mustCall());

// Test async file handle close
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'close-handle.txt'), 'close test');

  const fd = realVfs.openSync('/close-handle.txt', 'r');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  await handle.entry.close();
  assert.strictEqual(handle.entry.closed, true);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'close-handle.txt'));
})().then(common.mustCall());

// Test recursive mkdir
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  realVfs.mkdirSync('/deep/nested/dir', { recursive: true });
  assert.strictEqual(fs.existsSync(path.join(testDir, 'deep/nested/dir')), true);

  // Clean up
  fs.rmdirSync(path.join(testDir, 'deep/nested/dir'));
  fs.rmdirSync(path.join(testDir, 'deep/nested'));
  fs.rmdirSync(path.join(testDir, 'deep'));
}

// Test lstatSync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'lstat.txt'), 'lstat test');

  const stat = realVfs.lstatSync('/lstat.txt');
  assert.strictEqual(stat.isFile(), true);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'lstat.txt'));
}

// Test async lstat
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'async-lstat.txt'), 'async lstat');

  const stat = await realVfs.promises.lstat('/async-lstat.txt');
  assert.strictEqual(stat.isFile(), true);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-lstat.txt'));
})().then(common.mustCall());

// Test async copyFile
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'async-src.txt'), 'async copy');

  await realVfs.promises.copyFile('/async-src.txt', '/async-dest.txt');

  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-dest.txt')), true);
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'async-dest.txt'), 'utf8'), 'async copy');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-src.txt'));
  fs.unlinkSync(path.join(testDir, 'async-dest.txt'));
})().then(common.mustCall());

// Test async mkdir and rmdir
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  await realVfs.promises.mkdir('/async-dir');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-dir')), true);

  await realVfs.promises.rmdir('/async-dir');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-dir')), false);
})().then(common.mustCall());

// Test async rename
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'async-old.txt'), 'async rename');

  await realVfs.promises.rename('/async-old.txt', '/async-new.txt');

  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-old.txt')), false);
  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-new.txt')), true);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-new.txt'));
})().then(common.mustCall());

// Test async readdir
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.mkdirSync(path.join(testDir, 'async-readdir'), { recursive: true });
  fs.writeFileSync(path.join(testDir, 'async-readdir', 'file.txt'), 'content');

  const entries = await realVfs.promises.readdir('/async-readdir');
  assert.deepStrictEqual(entries, ['file.txt']);

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-readdir', 'file.txt'));
  fs.rmdirSync(path.join(testDir, 'async-readdir'));
})().then(common.mustCall());

// Test async unlink
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'async-unlink.txt'), 'to delete');

  await realVfs.promises.unlink('/async-unlink.txt');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'async-unlink.txt')), false);
})().then(common.mustCall());

// Test file handle readFile and writeFile
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'handle-rw.txt'), 'original');

  const fd = realVfs.openSync('/handle-rw.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Read via readFile
  const content = handle.entry.readFileSync('utf8');
  assert.strictEqual(content, 'original');

  // Write via writeFile
  handle.entry.writeFileSync('replaced');
  realVfs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(path.join(testDir, 'handle-rw.txt'), 'utf8'), 'replaced');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'handle-rw.txt'));
})().then(common.mustCall());

// Test async readFile and writeFile on handle
(async () => {
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));

  fs.writeFileSync(path.join(testDir, 'async-rw.txt'), 'async original');

  const fd = realVfs.openSync('/async-rw.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Async read
  const content = await handle.entry.readFile('utf8');
  assert.strictEqual(content, 'async original');

  // Async write
  await handle.entry.writeFile('async replaced');
  realVfs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(path.join(testDir, 'async-rw.txt'), 'utf8'), 'async replaced');

  // Clean up
  fs.unlinkSync(path.join(testDir, 'async-rw.txt'));
})().then(common.mustCall());
