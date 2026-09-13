'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { join } = require('path');
const { test } = require('node:test');

common.expectWarning(
  'ExperimentalWarning',
  'The mock.fs API is an experimental feature and might change at any time',
);

test('mock.fs() creates a mounted virtual file system', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'config.json': '{"key": "value"}',
      'data.txt': 'hello world',
    },
  });

  // The mock is mounted at a reserved mount point assigned by the VFS.
  assert.strictEqual(typeof mockFs.mountPoint, 'string');

  // Files are accessible via standard fs APIs under the mount point.
  const config = fs.readFileSync(join(mockFs.mountPoint, 'config.json'), 'utf8');
  assert.strictEqual(config, '{"key": "value"}');

  const data = fs.readFileSync(join(mockFs.mountPoint, 'data.txt'), 'utf8');
  assert.strictEqual(data, 'hello world');

  assert.strictEqual(fs.existsSync(join(mockFs.mountPoint, 'config.json')), true);
  assert.strictEqual(fs.existsSync(join(mockFs.mountPoint, 'nonexistent.txt')), false);
});

test('mock.fs() works without initial files', (t) => {
  const mockFs = t.mock.fs();

  const file1 = mockFs.addFile('file1.txt', 'content1');
  const file2 = mockFs.addFile('file2.txt', 'content2');

  assert.strictEqual(file1, join(mockFs.mountPoint, 'file1.txt'));
  assert.strictEqual(fs.readFileSync(file1, 'utf8'), 'content1');
  assert.strictEqual(fs.readFileSync(file2, 'utf8'), 'content2');
});

test('mock.fs() creates parent directories for nested files', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'deep/nested/dir/file.txt': 'nested content',
    },
  });

  const filePath = join(mockFs.mountPoint, 'deep', 'nested', 'dir', 'file.txt');
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'nested content');

  const dirStat = fs.statSync(join(mockFs.mountPoint, 'deep', 'nested'));
  assert.strictEqual(dirStat.isDirectory(), true);
});

test('mock.fs() supports adding directories', (t) => {
  const mockFs = t.mock.fs();

  const dir = mockFs.addDirectory('src');
  assert.strictEqual(dir, join(mockFs.mountPoint, 'src'));
  const file = mockFs.addFile('src/index.js', 'module.exports = "hello"');

  assert.strictEqual(fs.readFileSync(file, 'utf8'), 'module.exports = "hello"');

  const entries = fs.readdirSync(dir);
  assert.deepStrictEqual(entries, ['index.js']);
});

test('mock.fs() existsSync works correctly', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'existing.txt': 'content',
    },
  });

  // Via the mock context, relative to the mount point.
  assert.strictEqual(mockFs.existsSync('existing.txt'), true);
  assert.strictEqual(mockFs.existsSync('nonexistent.txt'), false);

  // Via standard fs.
  assert.strictEqual(fs.existsSync(join(mockFs.mountPoint, 'existing.txt')), true);
  assert.strictEqual(fs.existsSync(join(mockFs.mountPoint, 'nonexistent.txt')), false);
});

test('mock.fs() supports Buffer content', (t) => {
  const binaryData = Buffer.from([0x00, 0x01, 0x02, 0x03]);
  const mockFs = t.mock.fs({
    files: {
      'binary.bin': binaryData,
    },
  });

  const content = fs.readFileSync(join(mockFs.mountPoint, 'binary.bin'));
  assert.deepStrictEqual(content, binaryData);
});

test('mock.fs() is automatically cleaned up after test', async (t) => {
  let mountPoint;

  await t.test('subtest with mock', (st) => {
    const mockFs = st.mock.fs({
      files: {
        'temp.txt': 'temporary',
      },
    });
    mountPoint = mockFs.mountPoint;
    assert.strictEqual(fs.existsSync(join(mountPoint, 'temp.txt')), true);
  });

  // After the subtest, the mock file system is unmounted.
  assert.strictEqual(fs.existsSync(join(mountPoint, 'temp.txt')), false);
});

test('mock.fs() can be manually restored', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'file.txt': 'content',
    },
  });

  const filePath = join(mockFs.mountPoint, 'file.txt');
  assert.strictEqual(fs.existsSync(filePath), true);

  mockFs.restore();

  assert.strictEqual(mockFs.mountPoint, null);
  assert.strictEqual(fs.existsSync(filePath), false);
  assert.strictEqual(mockFs.existsSync('file.txt'), false);
  assert.throws(() => mockFs.addFile('other.txt', 'x'), {
    code: 'ERR_INVALID_STATE',
  });

  // Restoring again is a no-op.
  mockFs.restore();
});

test('mock.fs() exposes the underlying vfs', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'file.txt': 'content',
    },
  });

  const vfs = mockFs.vfs;
  assert.strictEqual(vfs.mounted, true);
  assert.strictEqual(vfs.mountPoint, mockFs.mountPoint);

  // The VFS instance can be used directly with absolute mounted paths.
  const content = vfs.readFileSync(join(mockFs.mountPoint, 'file.txt'), 'utf8');
  assert.strictEqual(content, 'content');
});

test('mock.fs() supports require() of virtual files', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'module.js': 'module.exports = { value: 42 };',
    },
  });

  const mod = require(join(mockFs.mountPoint, 'module.js'));
  assert.strictEqual(mod.value, 42);
});

test('mock.fs() supports import of virtual files', async (t) => {
  const mockFs = t.mock.fs({
    files: {
      'module.mjs': 'export const value = 43;',
    },
  });

  const { pathToFileURL } = require('url');
  const mod = await import(pathToFileURL(join(mockFs.mountPoint, 'module.mjs')));
  assert.strictEqual(mod.value, 43);
});

test('mock.fs() supports statSync', (t) => {
  const mockFs = t.mock.fs({
    files: {
      'file.txt': 'hello',
    },
  });

  const dir = mockFs.addDirectory('dir');

  const fileStat = fs.statSync(join(mockFs.mountPoint, 'file.txt'));
  assert.strictEqual(fileStat.isFile(), true);
  assert.strictEqual(fileStat.isDirectory(), false);
  assert.strictEqual(fileStat.size, 5);

  const dirStat = fs.statSync(dir);
  assert.strictEqual(dirStat.isFile(), false);
  assert.strictEqual(dirStat.isDirectory(), true);
});

test('multiple mock.fs() instances can coexist', (t) => {
  const mockFs1 = t.mock.fs({
    files: { 'file.txt': 'from mock1' },
  });

  const mockFs2 = t.mock.fs({
    files: { 'file.txt': 'from mock2' },
  });

  // Each mock gets its own reserved mount point.
  assert.notStrictEqual(mockFs1.mountPoint, mockFs2.mountPoint);

  assert.strictEqual(
    fs.readFileSync(join(mockFs1.mountPoint, 'file.txt'), 'utf8'),
    'from mock1');
  assert.strictEqual(
    fs.readFileSync(join(mockFs2.mountPoint, 'file.txt'), 'utf8'),
    'from mock2');
});

test('mock.fs() validates options', (t) => {
  assert.throws(
    () => t.mock.fs('invalid'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  assert.throws(
    () => t.mock.fs({ files: 'invalid' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  assert.throws(
    () => t.mock.fs({ files: { 42: null } }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  const mockFs = t.mock.fs();
  assert.throws(
    () => mockFs.addFile(42, 'content'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => mockFs.existsSync(null),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});
