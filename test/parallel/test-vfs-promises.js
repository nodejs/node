'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// Test callback-based readFile
{
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();

  myVfs.readFile('/nonexistent.txt', common.mustCall((err, data) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(data, undefined);
  }));
}

// Test callback-based readFile with directory
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/mydir', { recursive: true });

  myVfs.readFile('/mydir', common.mustCall((err, data) => {
    assert.strictEqual(err.code, 'EISDIR');
    assert.strictEqual(data, undefined);
  }));
}

// Test callback-based stat
{
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/file.txt', 'content');

  myVfs.lstat('/file.txt', common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), true);
  }));
}

// Test callback-based readdir
{
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/accessible.txt', 'content');

  myVfs.access('/accessible.txt', common.mustCall((err) => {
    assert.strictEqual(err, null);
  }));

  myVfs.access('/nonexistent.txt', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

// Test async dynamic content with callback API using provider.setContentProvider
{
  const myVfs = fs.createVirtual();
  myVfs.provider.setContentProvider('/async-dynamic.txt', async () => {
    return 'async content';
  });

  myVfs.readFile('/async-dynamic.txt', 'utf8', common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.strictEqual(data, 'async content');
  }));
}

// ==================== Promise API Tests ====================

// Test promises.readFile
(async () => {
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/lstat-file.txt', 'content');

  const stats = await myVfs.promises.lstat('/lstat-file.txt');
  assert.strictEqual(stats.isFile(), true);
})().then(common.mustCall());

// Test promises.readdir
(async () => {
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
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
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/access-test.txt', 'content');

  await myVfs.promises.access('/access-test.txt');

  await assert.rejects(
    myVfs.promises.access('/nonexistent'),
    { code: 'ENOENT' }
  );
})().then(common.mustCall());

// Test async dynamic content with promise API using provider.setContentProvider
(async () => {
  const myVfs = fs.createVirtual();
  let counter = 0;

  myVfs.provider.setContentProvider('/async-counter.txt', async () => {
    await new Promise((resolve) => setTimeout(resolve, 10));
    counter++;
    return `count: ${counter}`;
  });

  const data1 = await myVfs.promises.readFile('/async-counter.txt', 'utf8');
  assert.strictEqual(data1, 'count: 1');

  const data2 = await myVfs.promises.readFile('/async-counter.txt', 'utf8');
  assert.strictEqual(data2, 'count: 2');
})().then(common.mustCall());
