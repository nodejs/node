// Flags: --experimental-test-fs-mocks
'use strict';

// Tests core file operations of the mock file system feature.

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const fsPromises = require('node:fs/promises');
const { it, describe, mock } = require('node:test');

describe('MockFileSystem', () => {
  describe('enable/reset', () => {
    it('should throw if enabling twice', (t) => {
      t.mock.fs.enable({ files: {} });
      assert.throws(() => t.mock.fs.enable({ files: {} }), {
        code: 'ERR_INVALID_STATE',
      });
    });

    it('should not throw if reset without enable', (t) => {
      t.mock.fs.reset();
    });

    it('should restore fs after reset', (t) => {
      t.mock.fs.enable({
        files: { '/virtual/test.txt': 'content' },
        isolate: true,
      });
      assert.strictEqual(fs.readFileSync('/virtual/test.txt', 'utf8'), 'content');
      t.mock.fs.reset();
      assert.throws(() => fs.readFileSync('/virtual/test.txt'), { code: 'ENOENT' });
    });

    it('should support multiple enable/reset cycles', (t) => {
      // First cycle
      t.mock.fs.enable({
        files: { '/virtual/first.txt': 'first content' },
        isolate: true,
      });
      assert.strictEqual(fs.readFileSync('/virtual/first.txt', 'utf8'), 'first content');
      t.mock.fs.reset();

      // Second cycle with different files
      t.mock.fs.enable({
        files: { '/virtual/second.txt': 'second content' },
        isolate: true,
      });
      assert.strictEqual(fs.readFileSync('/virtual/second.txt', 'utf8'), 'second content');
      // First file should not exist
      assert.throws(() => fs.readFileSync('/virtual/first.txt'), { code: 'ENOENT' });
      t.mock.fs.reset();

      // Third cycle
      t.mock.fs.enable({
        files: { '/virtual/third.txt': 'third content' },
        isolate: true,
      });
      assert.strictEqual(fs.readFileSync('/virtual/third.txt', 'utf8'), 'third content');
    });
  });

  describe('validation', () => {
    it('should throw for invalid files option', (t) => {
      assert.throws(() => t.mock.fs.enable({ files: 'invalid' }), {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    });

    it('should throw for invalid isolate option', (t) => {
      assert.throws(() => t.mock.fs.enable({ files: {}, isolate: 'true' }), {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    });

    it('should throw for invalid file content', (t) => {
      assert.throws(() => t.mock.fs.enable({ files: { '/test': 123 } }), {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    });

    it('should throw for unsupported api option', (t) => {
      assert.throws(() => t.mock.fs.enable({ files: {}, apis: ['unsupported'] }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    });

    it('should prevent prototype pollution in paths', (t) => {
      for (const name of ['__proto__', 'constructor', 'prototype']) {
        assert.throws(() => t.mock.fs.enable({ files: { [`/path/${name}/file`]: 'x' } }), {
          code: 'ERR_INVALID_ARG_VALUE',
        });
        t.mock.fs.reset();
      }
    });

    it('should prevent prototype pollution via modified prototype', (t) => {
      const maliciousFiles = { __proto__: null };
      Object.setPrototypeOf(maliciousFiles, { polluted: true });
      assert.throws(() => t.mock.fs.enable({ files: maliciousFiles }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
      assert.strictEqual(Object.prototype.polluted, undefined);
    });

  });

  describe('readFile', () => {
    it('should read virtual file with string content', (t) => {
      t.mock.fs.enable({ files: { '/virtual/test.txt': 'Hello' } });
      assert.strictEqual(fs.readFileSync('/virtual/test.txt', 'utf8'), 'Hello');
    });

    it('should read virtual file with Buffer content', (t) => {
      const buf = Buffer.from('binary');
      t.mock.fs.enable({ files: { '/virtual/bin': buf } });
      assert.deepStrictEqual(fs.readFileSync('/virtual/bin'), buf);
    });

    it('should work with callback', (t, done) => {
      t.mock.fs.enable({ files: { '/virtual/cb.txt': 'callback' } });
      fs.readFile('/virtual/cb.txt', 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'callback');
        done();
      }));
    });

    it('should work with promises', async (t) => {
      t.mock.fs.enable({ files: { '/virtual/promise.txt': 'async' } });
      assert.strictEqual(await fsPromises.readFile('/virtual/promise.txt', 'utf8'), 'async');
    });

    it('should throw EISDIR for directory', (t) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      assert.throws(() => fs.readFileSync('/virtual/dir'), { code: 'EISDIR' });
    });

    it('should fall back to real fs in non-isolate mode', (t) => {
      t.mock.fs.enable({ files: { '/virtual/mock.txt': 'mocked' } });
      assert.ok(fs.readFileSync(__filename, 'utf8').includes('MockFileSystem'));
    });

    it('should throw ENOENT in isolate mode', (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      assert.throws(() => fs.readFileSync(__filename), { code: 'ENOENT' });
    });

    it('should pass ENOENT to callback in isolate mode', (t, done) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      fs.readFile('/nonexistent/file.txt', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ENOENT');
        done();
      }));
    });
  });

  describe('writeFile', () => {
    it('should write to virtual fs', (t) => {
      t.mock.fs.enable({ files: {} });
      fs.writeFileSync('/virtual/new.txt', 'new content');
      assert.strictEqual(fs.readFileSync('/virtual/new.txt', 'utf8'), 'new content');
    });

    it('should overwrite existing file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'old' } });
      fs.writeFileSync('/virtual/file.txt', 'new');
      assert.strictEqual(fs.readFileSync('/virtual/file.txt', 'utf8'), 'new');
    });

    it('should work with promises', async (t) => {
      t.mock.fs.enable({ files: {} });
      await fsPromises.writeFile('/virtual/async.txt', 'async');
      assert.strictEqual(fs.readFileSync('/virtual/async.txt', 'utf8'), 'async');
    });

    it('should always write to virtual fs even for real paths in non-isolate mode', (t) => {
      // In non-isolate mode, writes should still go to virtual fs
      // to prevent accidental writes to the real file system during tests
      t.mock.fs.enable({ files: {} });
      const testPath = '/tmp/mock-fs-test-write.txt';

      // Write to a path that could be real
      fs.writeFileSync(testPath, 'virtual content');

      // Should be readable from mock
      assert.strictEqual(fs.readFileSync(testPath, 'utf8'), 'virtual content');

      // After reset, the file should not exist (it was only in virtual fs)
      t.mock.fs.reset();
      // If the file existed before, this would read real content
      // If it didn't exist, this would throw ENOENT
      // Either way, it should NOT contain 'virtual content'
      try {
        const content = fs.readFileSync(testPath, 'utf8');
        // If we can read the file, it should NOT contain 'virtual content'
        // because writes should have gone to virtual fs, not real fs
        assert.ok(
          content !== 'virtual content',
          'Write should not have gone to real filesystem'
        );
      } catch (err) {
        assert.strictEqual(err.code, 'ENOENT');
      }
    });

    it('should work with callback', (t, done) => {
      t.mock.fs.enable({ files: {} });
      fs.writeFile('/virtual/cb.txt', 'callback content', common.mustSucceed(() => {
        assert.strictEqual(fs.readFileSync('/virtual/cb.txt', 'utf8'), 'callback content');
        done();
      }));
    });
  });

  describe('appendFile', () => {
    it('should append to existing file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/log.txt': 'line1\n' } });
      fs.appendFileSync('/virtual/log.txt', 'line2\n');
      assert.strictEqual(fs.readFileSync('/virtual/log.txt', 'utf8'), 'line1\nline2\n');
    });

    it('should create file if not exists', (t) => {
      t.mock.fs.enable({ files: {} });
      fs.appendFileSync('/virtual/new.txt', 'first');
      assert.strictEqual(fs.readFileSync('/virtual/new.txt', 'utf8'), 'first');
    });
  });

  describe('stat/lstat', () => {
    it('should return stats for file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'content' } });
      const stats = fs.statSync('/virtual/file.txt');
      assert.ok(stats.isFile());
      assert.strictEqual(stats.size, 7);
    });

    it('should return stats for directory', (t) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      assert.ok(fs.statSync('/virtual/dir').isDirectory());
    });

    it('should return undefined with throwIfNoEntry: false', (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      assert.strictEqual(fs.statSync('/none', { throwIfNoEntry: false }), undefined);
    });

    it('lstat should behave like stat (no symlink support)', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'x' } });
      const stat = fs.statSync('/virtual/file.txt');
      const lstat = fs.lstatSync('/virtual/file.txt');
      assert.strictEqual(stat.isFile(), lstat.isFile());
      assert.strictEqual(lstat.isSymbolicLink(), false);
    });

    it('should return stats object that is instanceof fs.Stats', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'content' } });
      const stats = fs.statSync('/virtual/file.txt');

      // Mock stats should pass instanceof check
      assert.strictEqual(stats instanceof fs.Stats, true);

      // And should still have all the expected methods
      assert.strictEqual(typeof stats.isFile, 'function');
      assert.strictEqual(typeof stats.isDirectory, 'function');
      assert.strictEqual(typeof stats.isBlockDevice, 'function');
      assert.strictEqual(typeof stats.isCharacterDevice, 'function');
      assert.strictEqual(typeof stats.isSymbolicLink, 'function');
      assert.strictEqual(typeof stats.isFIFO, 'function');
      assert.strictEqual(typeof stats.isSocket, 'function');

      // And expected properties
      assert.strictEqual(typeof stats.dev, 'number');
      assert.strictEqual(typeof stats.ino, 'number');
      assert.strictEqual(typeof stats.mode, 'number');
      assert.strictEqual(typeof stats.size, 'number');
      assert.ok(stats.atime instanceof Date);
      assert.ok(stats.mtime instanceof Date);
    });
  });

  describe('exists/access', () => {
    it('existsSync should return true for virtual file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/exists.txt': 'x' } });
      assert.strictEqual(fs.existsSync('/virtual/exists.txt'), true);
      assert.strictEqual(fs.existsSync('/virtual'), true);
    });

    it('existsSync should return false in isolate mode', (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      assert.strictEqual(fs.existsSync('/nonexistent'), false);
    });

    it('exists callback should return true for virtual file', (t, done) => {
      t.mock.fs.enable({ files: { '/virtual/exists.txt': 'x' } });
      fs.exists('/virtual/exists.txt', common.mustCall((exists) => {
        assert.strictEqual(exists, true);
        done();
      }));
    });

    it('exists callback should return true for virtual directory', (t, done) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      fs.exists('/virtual/dir', common.mustCall((exists) => {
        assert.strictEqual(exists, true);
        done();
      }));
    });

    it('exists callback should return false in isolate mode', (t, done) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      fs.exists('/nonexistent', common.mustCall((exists) => {
        assert.strictEqual(exists, false);
        done();
      }));
    });

    it('accessSync should not throw for virtual file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/access.txt': 'x' } });
      fs.accessSync('/virtual/access.txt');
    });

    it('accessSync should throw ENOENT in isolate mode', (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      assert.throws(() => fs.accessSync('/nonexistent'), { code: 'ENOENT' });
    });

    it('should pass ENOENT to access callback in isolate mode', (t, done) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      fs.access('/nonexistent', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ENOENT');
        done();
      }));
    });
  });

  describe('unlink', () => {
    it('should delete virtual file', (t) => {
      t.mock.fs.enable({ files: { '/virtual/delete.txt': 'x' } });
      fs.unlinkSync('/virtual/delete.txt');
      assert.strictEqual(fs.existsSync('/virtual/delete.txt'), false);
    });

    it('should throw EISDIR for directory', (t) => {
      t.mock.fs.enable({ files: { '/virtual/dir/file.txt': 'x' } });
      assert.throws(() => fs.unlinkSync('/virtual/dir'), { code: 'EISDIR' });
    });

    it('should pass ENOENT to unlink callback in isolate mode', (t, done) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      fs.unlink('/nonexistent/file.txt', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ENOENT');
        done();
      }));
    });
  });

  describe('apis option', () => {
    it('should only mock specified apis', (t) => {
      t.mock.fs.enable({
        files: { '/virtual/file.txt': 'content' },
        apis: ['readFile'],
      });
      // readFile should be mocked
      assert.strictEqual(fs.readFileSync('/virtual/file.txt', 'utf8'), 'content');
      // existsSync should NOT be mocked (not in apis list)
      assert.strictEqual(fs.existsSync('/virtual/file.txt'), false);
    });

    it('should mock multiple specified apis', (t) => {
      t.mock.fs.enable({
        files: { '/virtual/file.txt': 'content' },
        apis: ['readFile', 'writeFile', 'stat', 'exists'],
      });

      // All specified apis should work
      assert.strictEqual(fs.readFileSync('/virtual/file.txt', 'utf8'), 'content');
      assert.strictEqual(fs.existsSync('/virtual/file.txt'), true);
      assert.ok(fs.statSync('/virtual/file.txt').isFile());

      fs.writeFileSync('/virtual/new.txt', 'new content');
      assert.strictEqual(fs.readFileSync('/virtual/new.txt', 'utf8'), 'new content');

      // Unlink is NOT in the apis list, so it should use real fs
      // Since /virtual/new.txt doesn't exist on real fs, this would fail differently
      // We just verify it doesn't delete from our virtual fs by checking the file still exists
      // after trying to unlink (which will either fail or do nothing to virtual fs)
      try {
        fs.unlinkSync('/virtual/new.txt');
      } catch {
        // Expected - real fs doesn't have this file
      }
      // File should still exist in virtual fs since unlink wasn't mocked
      assert.strictEqual(fs.readFileSync('/virtual/new.txt', 'utf8'), 'new content');
    });

    it('should work with only directory-related apis', (t) => {
      t.mock.fs.enable({
        files: {},
        apis: ['mkdir', 'readdir', 'stat', 'exists'],
      });

      fs.mkdirSync('/virtual/dir', { recursive: true });
      assert.ok(fs.existsSync('/virtual/dir'));
      assert.ok(fs.statSync('/virtual/dir').isDirectory());
      assert.deepStrictEqual(fs.readdirSync('/virtual/dir'), []);
    });
  });

  describe('path handling', () => {
    it('should handle file:// URLs', (t) => {
      t.mock.fs.enable({ files: { '/virtual/url.txt': 'url content' } });
      const url = new URL('file:///virtual/url.txt');
      assert.strictEqual(fs.readFileSync(url, 'utf8'), 'url content');
    });

    it('should throw for non-file:// URLs', (t) => {
      t.mock.fs.enable({ files: { '/virtual/file.txt': 'content' } });
      assert.throws(() => fs.readFileSync(new URL('http://example.com/file.txt')), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    });

    it('should handle Buffer paths', (t) => {
      t.mock.fs.enable({ files: { '/virtual/buf.txt': 'buffer' } });
      assert.strictEqual(fs.readFileSync(Buffer.from('/virtual/buf.txt'), 'utf8'), 'buffer');
    });

    it('should handle Windows-style paths with backslashes', { skip: !common.isWindows }, (t) => {
      t.mock.fs.enable({
        files: { 'C:\\virtual\\test.txt': 'windows content' },
        isolate: true,
      });
      assert.strictEqual(fs.readFileSync('C:\\virtual\\test.txt', 'utf8'), 'windows content');
      assert.ok(fs.existsSync('C:\\virtual'));
    });

    it('should handle Windows-style paths with forward slashes', { skip: !common.isWindows }, (t) => {
      t.mock.fs.enable({
        files: { 'C:/virtual/forward.txt': 'forward slash content' },
        isolate: true,
      });
      // Node.js normalizes forward slashes to backslashes on Windows
      assert.strictEqual(fs.readFileSync('C:/virtual/forward.txt', 'utf8'), 'forward slash content');
      assert.strictEqual(fs.readFileSync('C:\\virtual\\forward.txt', 'utf8'), 'forward slash content');
    });

    it('should handle Windows file:// URLs', { skip: !common.isWindows }, (t) => {
      t.mock.fs.enable({
        files: { 'C:\\virtual\\url.txt': 'windows url content' },
        isolate: true,
      });
      const url = new URL('file:///C:/virtual/url.txt');
      assert.strictEqual(fs.readFileSync(url, 'utf8'), 'windows url content');
    });
  });

  describe('Symbol.dispose', () => {
    it('should support using syntax for automatic cleanup', (t) => {
      {
        using mockFs = t.mock.fs;
        mockFs.enable({
          files: { '/virtual/dispose.txt': 'content' },
          isolate: true,
        });
        assert.strictEqual(fs.readFileSync('/virtual/dispose.txt', 'utf8'), 'content');
      }
      // After scope ends, mock should be reset
      assert.throws(() => fs.readFileSync('/virtual/dispose.txt'), { code: 'ENOENT' });
    });
  });

  describe('auto-reset', () => {
    it('first test creates a virtual file', (t) => {
      t.mock.fs.enable({
        files: { '/virtual/auto-reset-test.txt': 'first test' },
        isolate: true,
      });
      assert.strictEqual(fs.readFileSync('/virtual/auto-reset-test.txt', 'utf8'), 'first test');
    });

    it('second test should not see file from first test', (t) => {
      // The mock from the previous test should have been automatically reset
      // If we enable mock with isolate, the file from previous test should not exist
      t.mock.fs.enable({
        files: {},
        isolate: true,
      });
      assert.throws(() => fs.readFileSync('/virtual/auto-reset-test.txt'), { code: 'ENOENT' });
    });
  });

  describe('readdir recursive option', () => {
    it('should throw error when recursive option is used', (t) => {
      t.mock.fs.enable({
        files: { '/virtual/dir/file.txt': 'content' },
      });
      assert.throws(
        () => fs.readdirSync('/virtual/dir', { recursive: true }),
        { code: 'ERR_INVALID_ARG_VALUE' }
      );
    });
  });

  describe('promise rejections in isolate mode', () => {
    it('should reject readFile with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.readFile('/nonexistent/file.txt'),
        { code: 'ENOENT' }
      );
    });

    it('should reject stat with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.stat('/nonexistent/file.txt'),
        { code: 'ENOENT' }
      );
    });

    it('should reject lstat with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.lstat('/nonexistent/file.txt'),
        { code: 'ENOENT' }
      );
    });

    it('should reject access with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.access('/nonexistent/file.txt'),
        { code: 'ENOENT' }
      );
    });

    it('should reject unlink with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.unlink('/nonexistent/file.txt'),
        { code: 'ENOENT' }
      );
    });

    it('should reject readdir with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.readdir('/nonexistent/dir'),
        { code: 'ENOENT' }
      );
    });

    it('should reject rmdir with ENOENT', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.rmdir('/nonexistent/dir'),
        { code: 'ENOENT' }
      );
    });

    it('should reject mkdir with ENOENT when parent missing', async (t) => {
      t.mock.fs.enable({ files: {}, isolate: true });
      await assert.rejects(
        fsPromises.mkdir('/nonexistent/parent/dir'),
        { code: 'ENOENT' }
      );
    });
  });

  describe('top-level mock.fs export', () => {
    it('should be accessible from top-level mock export', () => {
      // The mock.fs should be accessible from the top-level export
      assert.ok(mock.fs);
      assert.strictEqual(typeof mock.fs.enable, 'function');
      assert.strictEqual(typeof mock.fs.reset, 'function');
    });

    it('should work with top-level mock.fs', () => {
      mock.fs.enable({
        files: { '/virtual/top-level.txt': 'top level content' },
        isolate: true,
      });

      try {
        assert.strictEqual(
          fs.readFileSync('/virtual/top-level.txt', 'utf8'),
          'top level content'
        );
      } finally {
        // Important: must reset manually since not using test context
        mock.fs.reset();
      }
    });

    it('should require manual reset when using top-level mock', () => {
      mock.fs.enable({
        files: { '/virtual/manual-reset.txt': 'content' },
        isolate: true,
      });

      assert.strictEqual(
        fs.readFileSync('/virtual/manual-reset.txt', 'utf8'),
        'content'
      );

      mock.fs.reset();

      // After reset, file should not exist
      assert.throws(
        () => fs.readFileSync('/virtual/manual-reset.txt'),
        { code: 'ENOENT' }
      );
    });
  });

  // This test MUST be the last test in this file because it makes a property
  // non-configurable which cannot be undone in the same process.
  describe('non-configurable property handling', () => {
    it('should throw when trying to mock non-configurable fs property', (t) => {
      // Make the property non-configurable.
      Object.defineProperty(fs, 'readFileSync', {
        value: fs.readFileSync,
        writable: true,
        enumerable: true,
        configurable: false,
      });

      assert.throws(() => t.mock.fs.enable({ files: { '/test.txt': 'content' }, apis: ['readFile'] }), {
        code: 'ERR_INVALID_STATE',
        message: /Cannot mock fs\.readFileSync because it is non-configurable/,
      });
    });
  });
});
