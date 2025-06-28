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

// Errors from cleanup are thrown
// It is difficult to arrange for rmdir to fail on windows
if (!common.isWindows) {
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
