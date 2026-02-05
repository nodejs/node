// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test callback-based readFile
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.txt', 'hello world');

  myVfs.readFile('/test.txt', common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.ok(Buffer.isBuffer(data));
    assert.strictEqual(data.toString(), 'hello world');
  }));

  myVfs.readFile('/test.txt', 'utf8', common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.strictEqual(data, 'hello world');
  }));

  myVfs.readFile('/test.txt', { encoding: 'utf8' }, common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.strictEqual(data, 'hello world');
  }));
}

// Test callback-based readFile with non-existent file
{
  const myVfs = vfs.create();

  myVfs.readFile('/nonexistent.txt', common.mustCall((err, data) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(data, undefined);
  }));
}

// Test callback-based readFile with directory
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/mydir', { recursive: true });

  myVfs.readFile('/mydir', common.mustCall((err, data) => {
    assert.strictEqual(err.code, 'EISDIR');
    assert.strictEqual(data, undefined);
  }));
}

// Test callback-based stat
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir', { recursive: true });
  myVfs.writeFileSync('/file.txt', 'content');

  myVfs.stat('/file.txt', common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), true);
    assert.strictEqual(stats.isDirectory(), false);
    assert.strictEqual(stats.size, 7);
  }));

  myVfs.stat('/dir', common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), false);
    assert.strictEqual(stats.isDirectory(), true);
  }));

  myVfs.stat('/nonexistent', common.mustCall((err, stats) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(stats, undefined);
  }));
}

// Test callback-based lstat (same as stat for VFS)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'content');

  myVfs.lstat('/file.txt', common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), true);
  }));
}

// Test callback-based readdir
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir/subdir', { recursive: true });
  myVfs.writeFileSync('/dir/file1.txt', 'a');
  myVfs.writeFileSync('/dir/file2.txt', 'b');

  myVfs.readdir('/dir', common.mustCall((err, entries) => {
    assert.strictEqual(err, null);
    assert.deepStrictEqual(entries.sort(), ['file1.txt', 'file2.txt', 'subdir']);
  }));

  myVfs.readdir('/dir', { withFileTypes: true }, common.mustCall((err, entries) => {
    assert.strictEqual(err, null);
    assert.strictEqual(entries.length, 3);

    const file1 = entries.find((e) => e.name === 'file1.txt');
    assert.strictEqual(file1.isFile(), true);
    assert.strictEqual(file1.isDirectory(), false);

    const subdir = entries.find((e) => e.name === 'subdir');
    assert.strictEqual(subdir.isFile(), false);
    assert.strictEqual(subdir.isDirectory(), true);
  }));

  myVfs.readdir('/nonexistent', common.mustCall((err, entries) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(entries, undefined);
  }));

  myVfs.readdir('/dir/file1.txt', common.mustCall((err, entries) => {
    assert.strictEqual(err.code, 'ENOTDIR');
    assert.strictEqual(entries, undefined);
  }));
}

// Test callback-based realpath
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/path/to', { recursive: true });
  myVfs.writeFileSync('/path/to/file.txt', 'content');

  myVfs.realpath('/path/to/file.txt', common.mustCall((err, resolved) => {
    assert.strictEqual(err, null);
    assert.strictEqual(resolved, '/path/to/file.txt');
  }));

  myVfs.realpath('/path/to/../to/file.txt', common.mustCall((err, resolved) => {
    assert.strictEqual(err, null);
    assert.strictEqual(resolved, '/path/to/file.txt');
  }));

  myVfs.realpath('/nonexistent', common.mustCall((err, resolved) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(resolved, undefined);
  }));
}

// Test callback-based access
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/accessible.txt', 'content');

  myVfs.access('/accessible.txt', common.mustCall((err) => {
    assert.strictEqual(err, null);
  }));

  myVfs.access('/nonexistent.txt', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

// ==================== Promise API Tests ====================

// Test promises.readFile
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/promise-test.txt', 'promise content');

  const bufferData = await myVfs.promises.readFile('/promise-test.txt');
  assert.ok(Buffer.isBuffer(bufferData));
  assert.strictEqual(bufferData.toString(), 'promise content');

  const stringData = await myVfs.promises.readFile('/promise-test.txt', 'utf8');
  assert.strictEqual(stringData, 'promise content');

  const stringData2 = await myVfs.promises.readFile('/promise-test.txt', { encoding: 'utf8' });
  assert.strictEqual(stringData2, 'promise content');

  await assert.rejects(
    myVfs.promises.readFile('/nonexistent.txt'),
    { code: 'ENOENT' }
  );

  myVfs.mkdirSync('/promisedir', { recursive: true });
  await assert.rejects(
    myVfs.promises.readFile('/promisedir'),
    { code: 'EISDIR' }
  );
})().then(common.mustCall());

// Test promises.stat
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/stat-dir', { recursive: true });
  myVfs.writeFileSync('/stat-file.txt', 'hello');

  const fileStats = await myVfs.promises.stat('/stat-file.txt');
  assert.strictEqual(fileStats.isFile(), true);
  assert.strictEqual(fileStats.size, 5);

  const dirStats = await myVfs.promises.stat('/stat-dir');
  assert.strictEqual(dirStats.isDirectory(), true);

  await assert.rejects(
    myVfs.promises.stat('/nonexistent'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test promises.lstat
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/lstat-file.txt', 'content');

  const stats = await myVfs.promises.lstat('/lstat-file.txt');
  assert.strictEqual(stats.isFile(), true);
})().then(common.mustCall());

// Test promises.readdir
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pdir/sub', { recursive: true });
  myVfs.writeFileSync('/pdir/a.txt', 'a');
  myVfs.writeFileSync('/pdir/b.txt', 'b');

  const names = await myVfs.promises.readdir('/pdir');
  assert.deepStrictEqual(names.sort(), ['a.txt', 'b.txt', 'sub']);

  const dirents = await myVfs.promises.readdir('/pdir', { withFileTypes: true });
  assert.strictEqual(dirents.length, 3);
  const aFile = dirents.find((e) => e.name === 'a.txt');
  assert.strictEqual(aFile.isFile(), true);

  await assert.rejects(
    myVfs.promises.readdir('/nonexistent'),
    { code: 'ENOENT' }
  );

  await assert.rejects(
    myVfs.promises.readdir('/pdir/a.txt'),
    { code: 'ENOTDIR' }
  );
})().then(common.mustCall());

// Test promises.realpath
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real/path', { recursive: true });
  myVfs.writeFileSync('/real/path/file.txt', 'content');

  const resolved = await myVfs.promises.realpath('/real/path/file.txt');
  assert.strictEqual(resolved, '/real/path/file.txt');

  const normalized = await myVfs.promises.realpath('/real/path/../path/file.txt');
  assert.strictEqual(normalized, '/real/path/file.txt');

  await assert.rejects(
    myVfs.promises.realpath('/nonexistent'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test promises.access
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/access-test.txt', 'content');

  await myVfs.promises.access('/access-test.txt');

  await assert.rejects(
    myVfs.promises.access('/nonexistent'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test promises.writeFile
(async () => {
  const myVfs = vfs.create();

  await myVfs.promises.writeFile('/write-test.txt', 'async written');
  assert.strictEqual(myVfs.readFileSync('/write-test.txt', 'utf8'), 'async written');

  // Overwrite existing file
  await myVfs.promises.writeFile('/write-test.txt', 'overwritten');
  assert.strictEqual(myVfs.readFileSync('/write-test.txt', 'utf8'), 'overwritten');

  // Write with Buffer
  await myVfs.promises.writeFile('/buffer-write.txt', Buffer.from('buffer data'));
  assert.strictEqual(myVfs.readFileSync('/buffer-write.txt', 'utf8'), 'buffer data');
})().then(common.mustCall());

// Test promises.appendFile
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/append-test.txt', 'start');

  await myVfs.promises.appendFile('/append-test.txt', '-end');
  assert.strictEqual(myVfs.readFileSync('/append-test.txt', 'utf8'), 'start-end');

  // Append to non-existent file creates it
  await myVfs.promises.appendFile('/new-append.txt', 'new content');
  assert.strictEqual(myVfs.readFileSync('/new-append.txt', 'utf8'), 'new content');
})().then(common.mustCall());

// Test promises.mkdir
(async () => {
  const myVfs = vfs.create();

  await myVfs.promises.mkdir('/async-dir');
  const stat = myVfs.statSync('/async-dir');
  assert.strictEqual(stat.isDirectory(), true);

  // Recursive mkdir
  await myVfs.promises.mkdir('/async-dir/nested/deep', { recursive: true });
  assert.strictEqual(myVfs.statSync('/async-dir/nested/deep').isDirectory(), true);

  // Mkdir on existing directory throws without recursive
  await assert.rejects(
    myVfs.promises.mkdir('/async-dir'),
    { code: 'EEXIST' }
  );
})().then(common.mustCall());

// Test promises.unlink
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/unlink-test.txt', 'to delete');

  await myVfs.promises.unlink('/unlink-test.txt');
  assert.strictEqual(myVfs.existsSync('/unlink-test.txt'), false);

  await assert.rejects(
    myVfs.promises.unlink('/nonexistent.txt'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test promises.rmdir
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/rmdir-test');

  await myVfs.promises.rmdir('/rmdir-test');
  assert.strictEqual(myVfs.existsSync('/rmdir-test'), false);

  // Rmdir on non-empty directory throws
  myVfs.mkdirSync('/nonempty');
  myVfs.writeFileSync('/nonempty/file.txt', 'content');
  await assert.rejects(
    myVfs.promises.rmdir('/nonempty'),
    { code: 'ENOTEMPTY' }
  );
})().then(common.mustCall());

// Test promises.rename
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/rename-src.txt', 'rename me');

  await myVfs.promises.rename('/rename-src.txt', '/rename-dest.txt');
  assert.strictEqual(myVfs.existsSync('/rename-src.txt'), false);
  assert.strictEqual(myVfs.readFileSync('/rename-dest.txt', 'utf8'), 'rename me');
})().then(common.mustCall());

// Test promises.copyFile
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/copy-src.txt', 'copy me');

  await myVfs.promises.copyFile('/copy-src.txt', '/copy-dest.txt');
  assert.strictEqual(myVfs.readFileSync('/copy-dest.txt', 'utf8'), 'copy me');
  // Source still exists
  assert.strictEqual(myVfs.existsSync('/copy-src.txt'), true);

  await assert.rejects(
    myVfs.promises.copyFile('/nonexistent.txt', '/fail.txt'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test async truncate (via file handle)
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/truncate-test.txt', 'async content');

  const fd = myVfs.openSync('/truncate-test.txt', 'r+');
  const { getVirtualFd } = require('internal/vfs/fd');
  const handle = getVirtualFd(fd);

  await handle.entry.truncate(5);
  myVfs.closeSync(fd);
  assert.strictEqual(myVfs.readFileSync('/truncate-test.txt', 'utf8'), 'async');
})().then(common.mustCall());
