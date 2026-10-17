'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { isMainThread } = require('worker_threads');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Basic usage
{
  const result = fs.mkdtempDisposableSync(tmpdir.resolve('foo.'));

  assert.strictEqual(path.basename(result.path).length, 'foo.XXXXXX'.length);
  assert.strictEqual(path.dirname(result.path), tmpdir.path);
  assert(fs.existsSync(result.path));

  result.remove();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error
  result.remove();
}

// Usage with [Symbol.dispose]()
{
  const result = fs.mkdtempDisposableSync(tmpdir.resolve('foo.'));

  assert(fs.existsSync(result.path));

  result[Symbol.dispose]();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error
  result[Symbol.dispose]();
}

// `chdir`` does not affect removal
// Can't use chdir in workers
if (isMainThread) {
  const originalCwd = process.cwd();

  process.chdir(tmpdir.path);
  const first = fs.mkdtempDisposableSync('first.');
  const second = fs.mkdtempDisposableSync('second.');

  const fullFirstPath = path.join(tmpdir.path, first.path);
  const fullSecondPath = path.join(tmpdir.path, second.path);

  assert(fs.existsSync(fullFirstPath));
  assert(fs.existsSync(fullSecondPath));

  process.chdir(fullFirstPath);
  second.remove();

  assert(!fs.existsSync(fullSecondPath));

  process.chdir(tmpdir.path);
  first.remove();
  assert(!fs.existsSync(fullFirstPath));

  process.chdir(originalCwd);
}

// Buffer paths
{
  const prefix = tmpdir.resolve('foo.');
  for (const result of [
    fs.mkdtempDisposableSync(Buffer.from(prefix)),
    fs.mkdtempDisposableSync(prefix, { encoding: 'buffer' }),
  ]) {
    assert(Buffer.isBuffer(result.path));
    assert.strictEqual(path.dirname(result.path.toString()), tmpdir.path);
    assert(fs.existsSync(result.path));

    result.remove();

    assert(!fs.existsSync(result.path));
  }
}

// `chdir` does not affect removal of a relative Buffer path
// Can't use chdir in workers
if (isMainThread) {
  const originalCwd = process.cwd();

  process.chdir(tmpdir.path);
  const result = fs.mkdtempDisposableSync(Buffer.from('buffer.'));
  const fullPath = path.join(tmpdir.path, result.path.toString());

  assert(fs.existsSync(fullPath));

  process.chdir(originalCwd);
  result.remove();

  assert(!fs.existsSync(fullPath));
}

// Buffer paths that are not valid UTF-8 are removed as is
// macOS rejects such file names, and Windows converts paths from UTF-8
if (common.isLinux) {
  const prefix = Buffer.concat([Buffer.from(tmpdir.resolve('foo')), Buffer.from([0xff, 0x2e])]);
  const result = fs.mkdtempDisposableSync(prefix);

  assert.deepStrictEqual(result.path.subarray(0, prefix.length), prefix);
  assert(fs.existsSync(result.path));

  result.remove();

  assert(!fs.existsSync(result.path));
}

// Relative Buffer path under a non-ASCII cwd
// Can't use chdir in workers
if (isMainThread) {
  const originalCwd = process.cwd();
  const nonAscii = fs.mkdtempSync(path.join(tmpdir.path, '\u7528\u6237-'));

  process.chdir(nonAscii);
  const result = fs.mkdtempDisposableSync(Buffer.from('buffer.'));
  const fullPath = path.join(nonAscii, result.path.toString());

  assert(fs.existsSync(fullPath));

  process.chdir(originalCwd);
  result.remove();

  assert(!fs.existsSync(fullPath));
  fs.rmSync(nonAscii, { recursive: true });
}

// `..` after a symlink is resolved by the OS at creation. remove() has to
// use those bytes; lexical normalization points at a directory that was
// never created. Windows normalizes `..` before following the symlink.
if (!common.isWindows) {
  const outside = fs.mkdtempSync(path.join(tmpdir.path, 'outside-'));
  const parent = fs.mkdtempSync(path.join(tmpdir.path, 'parent-'));
  const link = path.join(parent, 'link');
  fs.symlinkSync(outside, link);

  const prefix = Buffer.from(`${link}/../foo.`);
  const result = fs.mkdtempDisposableSync(prefix);
  const baseName = path.basename(result.path.toString());
  const createdPath = path.join(path.dirname(outside), baseName);

  assert(fs.existsSync(createdPath));
  assert(!fs.existsSync(path.join(parent, baseName)));

  result.remove();

  assert(!fs.existsSync(createdPath));
  fs.rmSync(outside, { recursive: true });
  fs.rmSync(parent, { recursive: true });
}

// Errors from cleanup are thrown
// It is difficult to arrange for rmdir to fail on windows
if (!common.isWindows && process.getuid() !== 0) {
  const base = fs.mkdtempDisposableSync(tmpdir.resolve('foo.'));

  // On Unix we can prevent removal by making the parent directory read-only
  const child = fs.mkdtempDisposableSync(path.join(base.path, 'bar.'));

  const originalMode = fs.statSync(base.path).mode;
  fs.chmodSync(base.path, 0o444);

  assert.throws(() => {
    child.remove();
  }, /EACCES|EPERM/);

  fs.chmodSync(base.path, originalMode);

  // Removal works once permissions are reset
  child.remove();
  assert(!fs.existsSync(child.path));

  base.remove();
  assert(!fs.existsSync(base.path));
}
