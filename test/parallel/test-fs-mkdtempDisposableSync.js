'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

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

  // second removal does not throw error
  result.remove();
}

// Usage with [Symbol.dispose]()
{
  const result = fs.mkdtempDisposableSync(tmpdir.resolve('foo.'));

  assert(fs.existsSync(result.path));

  result[Symbol.dispose]();

  assert(!fs.existsSync(result.path));

  // second removal does not throw error
  result[Symbol.dispose]();
}

// chdir does not affect removal
{
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
{
  const base = fs.mkdtempDisposableSync(tmpdir.resolve('foo.'));

  if (common.isWindows) {
    // On Windows we can prevent removal by holding a file open
    const testFile = path.join(base.path, 'locked-file.txt');
    fs.writeFileSync(testFile, 'test');
    const fd = fs.openSync(testFile, 'r');

    assert.throws(() => {
      base.remove();
    }, /EBUSY|ENOTEMPTY|EPERM/);

    fs.closeSync(fd);
    fs.unlinkSync(testFile);

    // removal works once file is closed
    base.remove();
    assert(!fs.existsSync(base.path));
  } else {
    // On Unix we can prevent removal by making the parent directory read-only
    const child = fs.mkdtempDisposableSync(path.join(base.path, 'bar.'));

    const originalMode = fs.statSync(base.path).mode;
    fs.chmodSync(base.path, 0o444);

    assert.throws(() => {
      child.remove();
    }, /EACCES|EPERM/);

    fs.chmodSync(base.path, originalMode);

    // removal works once permissions are reset
    child.remove();
    assert(!fs.existsSync(child.path));

    base.remove();
    assert(!fs.existsSync(base.path));
  }
}
