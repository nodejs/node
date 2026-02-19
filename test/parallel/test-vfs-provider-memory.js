// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Test MemoryProvider can be instantiated directly
{
  const provider = new vfs.MemoryProvider();
  assert.strictEqual(provider.readonly, false);
  assert.strictEqual(provider.supportsSymlinks, true);
}

// Test creating VFS with MemoryProvider (default)
{
  const myVfs = vfs.create();
  assert.ok(myVfs);
  assert.strictEqual(myVfs.readonly, false);
  assert.ok(myVfs.provider instanceof vfs.MemoryProvider);
}

// Test creating VFS with explicit MemoryProvider
{
  const myVfs = vfs.create(new vfs.MemoryProvider());
  assert.ok(myVfs);
  assert.strictEqual(myVfs.readonly, false);
}

// Test reading and writing files
{
  const myVfs = vfs.create();

  // Write a file
  myVfs.writeFileSync('/hello.txt', 'Hello from VFS!');

  // Read it back
  assert.strictEqual(myVfs.readFileSync('/hello.txt', 'utf8'), 'Hello from VFS!');

  // Read as Buffer
  const buf = myVfs.readFileSync('/hello.txt');
  assert.ok(Buffer.isBuffer(buf));
  assert.strictEqual(buf.toString(), 'Hello from VFS!');

  // Overwrite
  myVfs.writeFileSync('/hello.txt', 'Overwritten');
  assert.strictEqual(myVfs.readFileSync('/hello.txt', 'utf8'), 'Overwritten');
}

// Test appendFile
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/append.txt', 'start');
  myVfs.appendFileSync('/append.txt', '-end');
  assert.strictEqual(myVfs.readFileSync('/append.txt', 'utf8'), 'start-end');

  // Append to non-existent file creates it
  myVfs.appendFileSync('/new-append.txt', 'new content');
  assert.strictEqual(myVfs.readFileSync('/new-append.txt', 'utf8'), 'new content');
}

// Test stat operations
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/stat-test.txt', 'content');
  myVfs.mkdirSync('/stat-dir', { recursive: true });

  const fileStat = myVfs.statSync('/stat-test.txt');
  assert.strictEqual(fileStat.isFile(), true);
  assert.strictEqual(fileStat.isDirectory(), false);
  assert.strictEqual(fileStat.size, 7);

  const dirStat = myVfs.statSync('/stat-dir');
  assert.strictEqual(dirStat.isFile(), false);
  assert.strictEqual(dirStat.isDirectory(), true);

  // Test ENOENT
  assert.throws(() => {
    myVfs.statSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test lstatSync (same as statSync for memory provider since no real symlink following)
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/lstat.txt', 'lstat test');

  const stat = myVfs.lstatSync('/lstat.txt');
  assert.strictEqual(stat.isFile(), true);
}

// Test readdirSync
{
  const myVfs = vfs.create();

  myVfs.mkdirSync('/readdir-test/subdir', { recursive: true });
  myVfs.writeFileSync('/readdir-test/a.txt', 'a');
  myVfs.writeFileSync('/readdir-test/b.txt', 'b');

  const entries = myVfs.readdirSync('/readdir-test');
  assert.deepStrictEqual(entries.sort(), ['a.txt', 'b.txt', 'subdir']);

  // With file types
  const dirents = myVfs.readdirSync('/readdir-test', { withFileTypes: true });
  assert.strictEqual(dirents.length, 3);

  const fileEntry = dirents.find((d) => d.name === 'a.txt');
  assert.ok(fileEntry);
  assert.strictEqual(fileEntry.isFile(), true);

  const dirEntry = dirents.find((d) => d.name === 'subdir');
  assert.ok(dirEntry);
  assert.strictEqual(dirEntry.isDirectory(), true);

  // ENOENT for non-existent directory
  assert.throws(() => {
    myVfs.readdirSync('/nonexistent');
  }, { code: 'ENOENT' });

  // ENOTDIR for file
  assert.throws(() => {
    myVfs.readdirSync('/readdir-test/a.txt');
  }, { code: 'ENOTDIR' });
}

// Test mkdir and rmdir
{
  const myVfs = vfs.create();

  myVfs.mkdirSync('/new-dir');
  assert.strictEqual(myVfs.existsSync('/new-dir'), true);
  assert.strictEqual(myVfs.statSync('/new-dir').isDirectory(), true);

  myVfs.rmdirSync('/new-dir');
  assert.strictEqual(myVfs.existsSync('/new-dir'), false);

  // EEXIST for existing directory
  myVfs.mkdirSync('/exists');
  assert.throws(() => {
    myVfs.mkdirSync('/exists');
  }, { code: 'EEXIST' });

  // ENOTEMPTY for non-empty directory
  myVfs.mkdirSync('/nonempty');
  myVfs.writeFileSync('/nonempty/file.txt', 'content');
  assert.throws(() => {
    myVfs.rmdirSync('/nonempty');
  }, { code: 'ENOTEMPTY' });
}

// Test recursive mkdir
{
  const myVfs = vfs.create();

  myVfs.mkdirSync('/deep/nested/dir', { recursive: true });
  assert.strictEqual(myVfs.existsSync('/deep/nested/dir'), true);
  assert.strictEqual(myVfs.statSync('/deep/nested/dir').isDirectory(), true);

  // Recursive on existing is OK
  myVfs.mkdirSync('/deep/nested/dir', { recursive: true });
  assert.strictEqual(myVfs.existsSync('/deep/nested/dir'), true);
}

// Test unlink
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/to-delete.txt', 'delete me');
  assert.strictEqual(myVfs.existsSync('/to-delete.txt'), true);

  myVfs.unlinkSync('/to-delete.txt');
  assert.strictEqual(myVfs.existsSync('/to-delete.txt'), false);

  // ENOENT for non-existent file
  assert.throws(() => {
    myVfs.unlinkSync('/nonexistent.txt');
  }, { code: 'ENOENT' });

  // EISDIR for directory
  myVfs.mkdirSync('/dir-to-unlink');
  assert.throws(() => {
    myVfs.unlinkSync('/dir-to-unlink');
  }, { code: 'EISDIR' });
}

// Test rename
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/old-name.txt', 'rename me');
  myVfs.renameSync('/old-name.txt', '/new-name.txt');

  assert.strictEqual(myVfs.existsSync('/old-name.txt'), false);
  assert.strictEqual(myVfs.existsSync('/new-name.txt'), true);
  assert.strictEqual(myVfs.readFileSync('/new-name.txt', 'utf8'), 'rename me');

  // ENOENT for non-existent source
  assert.throws(() => {
    myVfs.renameSync('/nonexistent.txt', '/dest.txt');
  }, { code: 'ENOENT' });
}

// Test copyFile
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/source.txt', 'copy me');
  myVfs.copyFileSync('/source.txt', '/dest.txt');

  assert.strictEqual(myVfs.existsSync('/source.txt'), true);
  assert.strictEqual(myVfs.existsSync('/dest.txt'), true);
  assert.strictEqual(myVfs.readFileSync('/dest.txt', 'utf8'), 'copy me');

  // ENOENT for non-existent source
  assert.throws(() => {
    myVfs.copyFileSync('/nonexistent.txt', '/fail.txt');
  }, { code: 'ENOENT' });
}

// Test realpathSync
{
  const myVfs = vfs.create();

  myVfs.mkdirSync('/path/to', { recursive: true });
  myVfs.writeFileSync('/path/to/real.txt', 'content');

  const resolved = myVfs.realpathSync('/path/to/real.txt');
  assert.strictEqual(resolved, '/path/to/real.txt');

  // With .. components
  const normalized = myVfs.realpathSync('/path/to/../to/real.txt');
  assert.strictEqual(normalized, '/path/to/real.txt');

  // ENOENT for non-existent
  assert.throws(() => {
    myVfs.realpathSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test existsSync
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/exists.txt', 'content');
  assert.strictEqual(myVfs.existsSync('/exists.txt'), true);
  assert.strictEqual(myVfs.existsSync('/not-exists.txt'), false);
}

// Test accessSync
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/accessible.txt', 'content');

  // Should not throw for existing file
  myVfs.accessSync('/accessible.txt');

  // Should throw ENOENT for non-existent
  assert.throws(() => {
    myVfs.accessSync('/nonexistent.txt');
  }, { code: 'ENOENT' });
}

// Test mounting MemoryProvider
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/mounted.txt', 'mounted content');
  myVfs.mount('/virtual');

  // Now should be able to read through standard fs
  const content = fs.readFileSync('/virtual/mounted.txt', 'utf8');
  assert.strictEqual(content, 'mounted content');

  myVfs.unmount();
}

// Test file handle operations via openSync
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/handle-test.txt', 'hello world');

  const fd = myVfs.openSync('/handle-test.txt', 'r');
  assert.ok(fd >= 10000);
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Read via file handle
  const buffer = Buffer.alloc(5);
  const bytesRead = handle.entry.readSync(buffer, 0, 5, 0);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buffer.toString(), 'hello');

  myVfs.closeSync(fd);
}

// Test file handle write operations
{
  const myVfs = vfs.create();

  const fd = myVfs.openSync('/write-handle.txt', 'w');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const buffer = Buffer.from('written via handle');
  const bytesWritten = handle.entry.writeSync(buffer, 0, buffer.length, 0);
  assert.strictEqual(bytesWritten, buffer.length);

  myVfs.closeSync(fd);

  // Verify content
  assert.strictEqual(myVfs.readFileSync('/write-handle.txt', 'utf8'), 'written via handle');
}

// Test file handle readFile and writeFile
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/handle-rw.txt', 'original');

  const fd = myVfs.openSync('/handle-rw.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Read via readFile
  const content = handle.entry.readFileSync('utf8');
  assert.strictEqual(content, 'original');

  // Write via writeFile
  handle.entry.writeFileSync('replaced');
  myVfs.closeSync(fd);

  assert.strictEqual(myVfs.readFileSync('/handle-rw.txt', 'utf8'), 'replaced');
}

// Test symlink operations
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/target.txt', 'target content');
  myVfs.symlinkSync('/target.txt', '/link.txt');

  // Reading through symlink should work
  assert.strictEqual(myVfs.readFileSync('/link.txt', 'utf8'), 'target content');

  // ReadlinkSync should return target
  assert.strictEqual(myVfs.readlinkSync('/link.txt'), '/target.txt');

  // Lstat on symlink should show it's a symlink
  const lstat = myVfs.lstatSync('/link.txt');
  assert.strictEqual(lstat.isSymbolicLink(), true);
}

// Test reading directory as file should fail
{
  const myVfs = vfs.create();

  myVfs.mkdirSync('/mydir', { recursive: true });
  assert.throws(() => {
    myVfs.readFileSync('/mydir');
  }, { code: 'EISDIR' });
}

// Test that readFileSync returns independent buffer copies
{
  const myVfs = vfs.create();

  myVfs.writeFileSync('/independent.txt', 'original content');

  const buf1 = myVfs.readFileSync('/independent.txt');
  const buf2 = myVfs.readFileSync('/independent.txt');

  // Both should have the same content
  assert.deepStrictEqual(buf1, buf2);

  // Mutating one should not affect the other
  buf1[0] = 0xFF;
  assert.notDeepStrictEqual(buf1, buf2);
  assert.strictEqual(buf2.toString(), 'original content');

  // A third read should still return the original content
  const buf3 = myVfs.readFileSync('/independent.txt');
  assert.strictEqual(buf3.toString(), 'original content');
}

// ==================== Async Operations ====================

// Test async read and write operations
(async () => {
  const myVfs = vfs.create();

  await myVfs.promises.writeFile('/async-test.txt', 'async content');
  const content = await myVfs.promises.readFile('/async-test.txt', 'utf8');
  assert.strictEqual(content, 'async content');

  const stat = await myVfs.promises.stat('/async-test.txt');
  assert.strictEqual(stat.isFile(), true);

  await myVfs.promises.unlink('/async-test.txt');
  assert.strictEqual(myVfs.existsSync('/async-test.txt'), false);
})().then(common.mustCall());

// Test async lstat
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-lstat.txt', 'async lstat');

  const stat = await myVfs.promises.lstat('/async-lstat.txt');
  assert.strictEqual(stat.isFile(), true);
})().then(common.mustCall());

// Test async copyFile
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-src.txt', 'async copy');

  await myVfs.promises.copyFile('/async-src.txt', '/async-dest.txt');

  assert.strictEqual(myVfs.existsSync('/async-dest.txt'), true);
  assert.strictEqual(myVfs.readFileSync('/async-dest.txt', 'utf8'), 'async copy');
})().then(common.mustCall());

// Test async mkdir and rmdir
(async () => {
  const myVfs = vfs.create();

  await myVfs.promises.mkdir('/async-dir');
  assert.strictEqual(myVfs.existsSync('/async-dir'), true);

  await myVfs.promises.rmdir('/async-dir');
  assert.strictEqual(myVfs.existsSync('/async-dir'), false);
})().then(common.mustCall());

// Test async rename
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-old.txt', 'async rename');

  await myVfs.promises.rename('/async-old.txt', '/async-new.txt');

  assert.strictEqual(myVfs.existsSync('/async-old.txt'), false);
  assert.strictEqual(myVfs.existsSync('/async-new.txt'), true);
})().then(common.mustCall());

// Test async readdir
(async () => {
  const myVfs = vfs.create();

  myVfs.mkdirSync('/async-readdir', { recursive: true });
  myVfs.writeFileSync('/async-readdir/file.txt', 'content');

  const entries = await myVfs.promises.readdir('/async-readdir');
  assert.deepStrictEqual(entries, ['file.txt']);
})().then(common.mustCall());

// Test async appendFile
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-append.txt', 'start');

  await myVfs.promises.appendFile('/async-append.txt', '-end');
  assert.strictEqual(myVfs.readFileSync('/async-append.txt', 'utf8'), 'start-end');
})().then(common.mustCall());

// Test async access
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-access.txt', 'content');

  await myVfs.promises.access('/async-access.txt');

  await assert.rejects(
    myVfs.promises.access('/nonexistent.txt'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test async realpath
(async () => {
  const myVfs = vfs.create();

  myVfs.mkdirSync('/async-real/path', { recursive: true });
  myVfs.writeFileSync('/async-real/path/file.txt', 'content');

  const resolved = await myVfs.promises.realpath('/async-real/path/file.txt');
  assert.strictEqual(resolved, '/async-real/path/file.txt');
})().then(common.mustCall());

// Test async file handle read
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-handle.txt', 'async read test');

  const fd = myVfs.openSync('/async-handle.txt', 'r');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const buffer = Buffer.alloc(10);
  const result = await handle.entry.read(buffer, 0, 10, 0);
  assert.strictEqual(result.bytesRead, 10);
  assert.strictEqual(buffer.toString(), 'async read');

  myVfs.closeSync(fd);
})().then(common.mustCall());

// Test async file handle write
(async () => {
  const myVfs = vfs.create();

  const fd = myVfs.openSync('/async-write.txt', 'w');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const buffer = Buffer.from('async write');
  const result = await handle.entry.write(buffer, 0, buffer.length, 0);
  assert.strictEqual(result.bytesWritten, buffer.length);

  myVfs.closeSync(fd);

  // Verify content
  assert.strictEqual(myVfs.readFileSync('/async-write.txt', 'utf8'), 'async write');
})().then(common.mustCall());

// Test async file handle stat
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/stat-handle.txt', 'stat test');

  const fd = myVfs.openSync('/stat-handle.txt', 'r');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  const stat = await handle.entry.stat();
  assert.strictEqual(stat.isFile(), true);

  myVfs.closeSync(fd);
})().then(common.mustCall());

// Test async file handle truncate
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/truncate-handle.txt', 'truncate this');

  const fd = myVfs.openSync('/truncate-handle.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  await handle.entry.truncate(8);
  myVfs.closeSync(fd);

  // Verify content was truncated
  assert.strictEqual(myVfs.readFileSync('/truncate-handle.txt', 'utf8'), 'truncate');
})().then(common.mustCall());

// Test async file handle close
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/close-handle.txt', 'close test');

  const fd = myVfs.openSync('/close-handle.txt', 'r');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  await handle.entry.close();
  assert.strictEqual(handle.entry.closed, true);
})().then(common.mustCall());

// Test async readFile and writeFile on handle
(async () => {
  const myVfs = vfs.create();

  myVfs.writeFileSync('/async-rw.txt', 'async original');

  const fd = myVfs.openSync('/async-rw.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Async read
  const content = await handle.entry.readFile('utf8');
  assert.strictEqual(content, 'async original');

  // Async write
  await handle.entry.writeFile('async replaced');
  myVfs.closeSync(fd);

  assert.strictEqual(myVfs.readFileSync('/async-rw.txt', 'utf8'), 'async replaced');
})().then(common.mustCall());

// ==================== Readonly Mode ====================

// Test MemoryProvider readonly mode
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'content');
  myVfs.mkdirSync('/dir', { recursive: true });

  // Set to readonly
  myVfs.provider.setReadOnly();
  assert.strictEqual(myVfs.provider.readonly, true);

  // Read operations should still work
  assert.strictEqual(myVfs.readFileSync('/file.txt', 'utf8'), 'content');
  assert.strictEqual(myVfs.existsSync('/file.txt'), true);
  assert.ok(myVfs.statSync('/file.txt'));

  // Write operations should throw EROFS
  assert.throws(() => {
    myVfs.writeFileSync('/file.txt', 'new content');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.writeFileSync('/new.txt', 'content');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.appendFileSync('/file.txt', 'more');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.mkdirSync('/newdir');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.unlinkSync('/file.txt');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.rmdirSync('/dir');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.renameSync('/file.txt', '/renamed.txt');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.copyFileSync('/file.txt', '/copy.txt');
  }, { code: 'EROFS' });

  assert.throws(() => {
    myVfs.symlinkSync('/file.txt', '/link');
  }, { code: 'EROFS' });
}

// Test async operations on readonly MemoryProvider
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/readonly.txt', 'content');
  myVfs.provider.setReadOnly();

  await assert.rejects(
    myVfs.promises.writeFile('/readonly.txt', 'new'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.appendFile('/readonly.txt', 'more'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.mkdir('/newdir'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.unlink('/readonly.txt'),
    { code: 'EROFS' }
  );

  await assert.rejects(
    myVfs.promises.copyFile('/readonly.txt', '/copy.txt'),
    { code: 'EROFS' }
  );
})().then(common.mustCall());
