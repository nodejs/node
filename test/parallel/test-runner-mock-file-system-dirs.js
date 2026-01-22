// Flags: --experimental-test-fs-mocks
'use strict';

// Tests directory operations of the mock file system feature.

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const fsPromises = require('node:fs/promises');
const { it, describe } = require('node:test');

describe('MockFileSystem - directories', () => {
  describe('mkdir', () => {
    it('should create directory', (t) => {
      t.mock.fs.enable({ files: {} });
      fs.mkdirSync('/virtual/newdir', { recursive: true });
      assert.ok(fs.statSync('/virtual/newdir').isDirectory());
    });

    it('should create nested directories with recursive', (t) => {
      t.mock.fs.enable({ files: {} });
      fs.mkdirSync('/virtual/a/b/c', { recursive: true });
      assert.ok(fs.existsSync('/virtual/a/b/c'));
    });

    it('should throw EEXIST if exists', (t) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      assert.throws(() => fs.mkdirSync('/virtual/dir'), { code: 'EEXIST' });
    });

    it('should not throw with recursive if exists', (t) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      fs.mkdirSync('/virtual/dir', { recursive: true });
    });

    it('should not create real directories in non-isolate mode (sync)', (t) => {
      t.mock.fs.enable({ files: {} });
      const testPath = '/tmp/mock-fs-test-no-leak-' + Date.now();

      // Should fail because parent doesn't exist in virtual fs
      assert.throws(() => fs.mkdirSync(testPath), { code: 'ENOENT' });

      // Verify nothing was created on real fs
      t.mock.fs.reset();
      assert.strictEqual(fs.existsSync(testPath), false);
    });

    it('should not create real directories in non-isolate mode (callback)', (t, done) => {
      t.mock.fs.enable({ files: {} });
      const testPath = '/tmp/mock-fs-test-no-leak-cb-' + Date.now();

      fs.mkdir(testPath, common.mustCall((err) => {
        assert.strictEqual(err.code, 'ENOENT');
        t.mock.fs.reset();
        assert.strictEqual(fs.existsSync(testPath), false);
        done();
      }));
    });

    it('should not create real directories in non-isolate mode (promises)', async (t) => {
      t.mock.fs.enable({ files: {} });
      const testPath = '/tmp/mock-fs-test-no-leak-promise-' + Date.now();

      // Should fail because parent doesn't exist in virtual fs
      await assert.rejects(fsPromises.mkdir(testPath), { code: 'ENOENT' });

      // Verify nothing was created on real fs
      t.mock.fs.reset();
      assert.strictEqual(fs.existsSync(testPath), false);
    });

    it('should restore original mkdirSync after reset', (t) => {
      const originalMkdirSync = fs.mkdirSync;
      t.mock.fs.enable({ files: {} });

      // mkdirSync should be mocked now
      assert.notStrictEqual(fs.mkdirSync, originalMkdirSync);

      t.mock.fs.reset();

      // mkdirSync should be restored
      assert.strictEqual(fs.mkdirSync, originalMkdirSync);
    });
  });

  describe('rmdir', () => {
    it('should remove empty directory', (t) => {
      t.mock.fs.enable({ files: {} });
      fs.mkdirSync('/virtual/empty', { recursive: true });
      fs.rmdirSync('/virtual/empty');
      assert.strictEqual(fs.existsSync('/virtual/empty'), false);
    });

    it('should throw ENOTEMPTY if not empty', (t) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      assert.throws(() => fs.rmdirSync('/virtual/dir'), { code: 'ENOTEMPTY' });
    });

    it('should throw ENOTDIR for file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'x' } });
      assert.throws(() => fs.rmdirSync('/virtual/file.txt'), { code: 'ENOTDIR' });
    });

    it('should work with promises', async (t) => {
      t.mock.fs.enable({ files: {} });
      fs.mkdirSync('/virtual/async-rm', { recursive: true });
      await fsPromises.rmdir('/virtual/async-rm');
      assert.strictEqual(fs.existsSync('/virtual/async-rm'), false);
    });
  });

  describe('readdir', () => {
    it('should list directory contents', (t) => {
      t.mock.fs.enable({
        files: {
          '/virtual/dir/a.txt': 'a',
          '/virtual/dir/b.txt': 'b',
          '/virtual/dir/sub/c.txt': 'c',
        },
      });
      const entries = fs.readdirSync('/virtual/dir').sort();
      assert.deepStrictEqual(entries, ['a.txt', 'b.txt', 'sub']);
    });

    it('should return dirents with withFileTypes', (t) => {
      t.mock.fs.enable({
        files: {
          '/virtual/dir/file.txt': 'x',
          '/virtual/dir/sub/nested.txt': 'y',
        },
      });
      const entries = fs.readdirSync('/virtual/dir', { withFileTypes: true });
      const file = entries.find((e) => e.name === 'file.txt');
      const dir = entries.find((e) => e.name === 'sub');
      assert.ok(file.isFile());
      assert.ok(dir.isDirectory());
    });

    it('should return correct path and parentPath in dirents', (t) => {
      t.mock.fs.enable({
        files: {
          '/virtual/parent/file.txt': 'content',
          '/virtual/parent/subdir/nested.txt': 'nested',
        },
      });
      const entries = fs.readdirSync('/virtual/parent', { withFileTypes: true });

      for (const entry of entries) {
        // Both path and parentPath should point to the parent directory
        assert.strictEqual(entry.path, '/virtual/parent');
        assert.strictEqual(entry.parentPath, '/virtual/parent');
      }

      // Verify the file entry
      const file = entries.find((e) => e.name === 'file.txt');
      assert.ok(file);
      assert.strictEqual(file.name, 'file.txt');

      // Verify the directory entry
      const subdir = entries.find((e) => e.name === 'subdir');
      assert.ok(subdir);
      assert.strictEqual(subdir.name, 'subdir');
    });

    it('dirents should have all expected methods', (t) => {
      t.mock.fs.enable({
        files: { '/virtual/dir/file.txt': 'x' },
      });
      const entries = fs.readdirSync('/virtual/dir', { withFileTypes: true });
      const entry = entries[0];

      // Verify all dirent methods exist and return correct types
      assert.strictEqual(typeof entry.isFile, 'function');
      assert.strictEqual(typeof entry.isDirectory, 'function');
      assert.strictEqual(typeof entry.isBlockDevice, 'function');
      assert.strictEqual(typeof entry.isCharacterDevice, 'function');
      assert.strictEqual(typeof entry.isSymbolicLink, 'function');
      assert.strictEqual(typeof entry.isFIFO, 'function');
      assert.strictEqual(typeof entry.isSocket, 'function');

      // For a file, these should return expected values
      assert.strictEqual(entry.isFile(), true);
      assert.strictEqual(entry.isDirectory(), false);
      assert.strictEqual(entry.isBlockDevice(), false);
      assert.strictEqual(entry.isCharacterDevice(), false);
      assert.strictEqual(entry.isSymbolicLink(), false);
      assert.strictEqual(entry.isFIFO(), false);
      assert.strictEqual(entry.isSocket(), false);
    });

    it('should throw ENOTDIR for file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'x' } });
      assert.throws(() => fs.readdirSync('/virtual/file.txt'), { code: 'ENOTDIR' });
    });

    it('should return empty array for empty dir', (t) => {
      t.mock.fs.enable({ files: {} });
      fs.mkdirSync('/virtual/empty', { recursive: true });
      assert.deepStrictEqual(fs.readdirSync('/virtual/empty'), []);
    });

    it('should work with callback', (t, done) => {
      t.mock.fs.enable({ files: { '/virtual/cb/file.txt': 'x' } });
      fs.readdir('/virtual/cb', common.mustSucceed((entries) => {
        assert.deepStrictEqual(entries, ['file.txt']);
        done();
      }));
    });

    it('should work with callback and withFileTypes', (t, done) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      fs.readdir('/virtual/dir', { withFileTypes: true }, common.mustSucceed((entries) => {
        assert.ok(entries[0].isFile());
        done();
      }));
    });
  });

  describe('auto-created parent directories', () => {
    it('should create all parent directories', (t) => {
      t.mock.fs.enable({ files: { '/a/b/c/d/file.txt': 'deep' } });
      assert.ok(fs.existsSync('/a'));
      assert.ok(fs.existsSync('/a/b'));
      assert.ok(fs.existsSync('/a/b/c'));
      assert.ok(fs.existsSync('/a/b/c/d'));
      assert.ok(fs.statSync('/a/b/c').isDirectory());
    });
  });

  describe('root path', () => {
    it('should handle root path readdir', { skip: common.isWindows }, (t) => {
      t.mock.fs.enable({
        files: { '/virtual/file.txt': 'content' },
        isolate: true,
      });
      const entries = fs.readdirSync('/');
      assert.ok(entries.includes('virtual'));
    });
  });
});
