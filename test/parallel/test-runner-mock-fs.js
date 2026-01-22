'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const { test } = require('node:test');

// Test basic mock.fs() functionality
test('mock.fs() creates a virtual file system', (t) => {
  const mockFs = t.mock.fs({
    prefix: '/test-vfs',
    files: {
      '/config.json': '{"key": "value"}',
      '/data.txt': 'hello world',
    },
  });

  // Verify the mock was created with correct prefix
  assert.strictEqual(mockFs.prefix, '/test-vfs');

  // Verify files are accessible via standard fs APIs
  const config = fs.readFileSync('/test-vfs/config.json', 'utf8');
  assert.strictEqual(config, '{"key": "value"}');

  const data = fs.readFileSync('/test-vfs/data.txt', 'utf8');
  assert.strictEqual(data, 'hello world');

  // Verify existsSync works
  assert.strictEqual(fs.existsSync('/test-vfs/config.json'), true);
  assert.strictEqual(fs.existsSync('/test-vfs/nonexistent.txt'), false);
});

// Test mock.fs() with default prefix
test('mock.fs() uses /mock as default prefix', (t) => {
  const mockFs = t.mock.fs({
    files: {
      '/file.txt': 'content',
    },
  });

  assert.strictEqual(mockFs.prefix, '/mock');
  assert.strictEqual(fs.readFileSync('/mock/file.txt', 'utf8'), 'content');
});

// Test mock.fs() without initial files
test('mock.fs() works without initial files', (t) => {
  const mockFs = t.mock.fs({ prefix: '/empty-vfs' });

  // Add files dynamically
  mockFs.addFile('/file1.txt', 'content1');
  mockFs.addFile('/file2.txt', 'content2');

  assert.strictEqual(fs.readFileSync('/empty-vfs/file1.txt', 'utf8'), 'content1');
  assert.strictEqual(fs.readFileSync('/empty-vfs/file2.txt', 'utf8'), 'content2');
});

// Test mock.fs() addDirectory
test('mock.fs() supports adding directories', (t) => {
  const mockFs = t.mock.fs({ prefix: '/dir-test' });

  mockFs.addDirectory('/src');
  mockFs.addFile('/src/index.js', 'module.exports = "hello"');

  const content = fs.readFileSync('/dir-test/src/index.js', 'utf8');
  assert.strictEqual(content, 'module.exports = "hello"');

  const entries = fs.readdirSync('/dir-test/src');
  assert.strictEqual(entries.length, 1);
  assert.strictEqual(entries[0], 'index.js');
});

// Test mock.fs() existsSync
test('mock.fs() existsSync works correctly', (t) => {
  const mockFs = t.mock.fs({
    prefix: '/exists-test',
    files: {
      '/existing.txt': 'content',
    },
  });

  // Test via mockFs context
  assert.strictEqual(mockFs.existsSync('/existing.txt'), true);
  assert.strictEqual(mockFs.existsSync('/nonexistent.txt'), false);

  // Test via standard fs
  assert.strictEqual(fs.existsSync('/exists-test/existing.txt'), true);
  assert.strictEqual(fs.existsSync('/exists-test/nonexistent.txt'), false);
});

// Test mock.fs() with Buffer content
test('mock.fs() supports Buffer content', (t) => {
  const binaryData = Buffer.from([0x00, 0x01, 0x02, 0x03]);
  t.mock.fs({
    prefix: '/buffer-test',
    files: {
      '/binary.bin': binaryData,
    },
  });

  const content = fs.readFileSync('/buffer-test/binary.bin');
  assert.deepStrictEqual(content, binaryData);
});

// Test mock.fs() automatic cleanup
test('mock.fs() is automatically cleaned up after test', async (t) => {
  // Create a mock within a subtest
  await t.test('subtest with mock', (st) => {
    st.mock.fs({
      prefix: '/cleanup-test',
      files: {
        '/temp.txt': 'temporary',
      },
    });
    assert.strictEqual(fs.existsSync('/cleanup-test/temp.txt'), true);
  });

  // After subtest, the mock should be cleaned up
  assert.strictEqual(fs.existsSync('/cleanup-test/temp.txt'), false);
});

// Test mock.fs() manual restore
test('mock.fs() can be manually restored', (t) => {
  const mockFs = t.mock.fs({
    prefix: '/manual-restore',
    files: {
      '/file.txt': 'content',
    },
  });

  assert.strictEqual(fs.existsSync('/manual-restore/file.txt'), true);

  // Manually restore
  mockFs.restore();

  // File should no longer be accessible
  assert.strictEqual(fs.existsSync('/manual-restore/file.txt'), false);
});

// Test mock.fs() vfs property
test('mock.fs() exposes vfs property', (t) => {
  const mockFs = t.mock.fs({
    prefix: '/vfs-prop-test',
    files: {
      '/file.txt': 'content',
    },
  });

  // Access the underlying VFS
  const vfs = mockFs.vfs;
  assert.ok(vfs);
  assert.strictEqual(typeof vfs.addFile, 'function');
  assert.strictEqual(typeof vfs.readFileSync, 'function');

  // Use VFS directly (requires full path including mount point)
  const content = vfs.readFileSync('/vfs-prop-test/file.txt', 'utf8');
  assert.strictEqual(content, 'content');
});

// Test mock.fs() with dynamic file content
test('mock.fs() supports dynamic file content', (t) => {
  let counter = 0;
  const mockFs = t.mock.fs({ prefix: '/dynamic-test' });

  mockFs.addFile('/counter.txt', () => {
    counter++;
    return String(counter);
  });

  // Each read should call the function
  assert.strictEqual(fs.readFileSync('/dynamic-test/counter.txt', 'utf8'), '1');
  assert.strictEqual(fs.readFileSync('/dynamic-test/counter.txt', 'utf8'), '2');
  assert.strictEqual(fs.readFileSync('/dynamic-test/counter.txt', 'utf8'), '3');
});

// Test require from mock.fs()
test('mock.fs() supports require() from virtual files', (t) => {
  t.mock.fs({
    prefix: '/require-test',
    files: {
      '/module.js': 'module.exports = { value: 42 };',
    },
  });

  const mod = require('/require-test/module.js');
  assert.strictEqual(mod.value, 42);
});

// Test stat from mock.fs()
test('mock.fs() supports statSync', (t) => {
  const mockFs = t.mock.fs({
    prefix: '/stat-test',
    files: {
      '/file.txt': 'hello',
    },
  });

  mockFs.addDirectory('/dir');

  const fileStat = fs.statSync('/stat-test/file.txt');
  assert.strictEqual(fileStat.isFile(), true);
  assert.strictEqual(fileStat.isDirectory(), false);
  assert.strictEqual(fileStat.size, 5);

  const dirStat = fs.statSync('/stat-test/dir');
  assert.strictEqual(dirStat.isFile(), false);
  assert.strictEqual(dirStat.isDirectory(), true);
});

// Test multiple mock.fs() instances
test('multiple mock.fs() instances can coexist', (t) => {
  t.mock.fs({
    prefix: '/mock1',
    files: { '/file.txt': 'from mock1' },
  });

  t.mock.fs({
    prefix: '/mock2',
    files: { '/file.txt': 'from mock2' },
  });

  assert.strictEqual(fs.readFileSync('/mock1/file.txt', 'utf8'), 'from mock1');
  assert.strictEqual(fs.readFileSync('/mock2/file.txt', 'utf8'), 'from mock2');
});

// Test that options validation works
test('mock.fs() validates options', (t) => {
  assert.throws(
    () => t.mock.fs('invalid'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  assert.throws(
    () => t.mock.fs({ files: 'invalid' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});
