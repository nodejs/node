'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

async function basicUsage() {
  const result = await fsPromises.mkdtempDisposable(tmpdir.resolve('foo.'));

  assert.strictEqual(path.basename(result.path).length, 'foo.XXXXXX'.length);
  assert.strictEqual(path.dirname(result.path), tmpdir.path);
  assert(fs.existsSync(result.path));

  await result.remove();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error
  result.remove();
}

async function symbolAsyncDispose() {
  const result = await fsPromises.mkdtempDisposable(tmpdir.resolve('foo.'));

  assert(fs.existsSync(result.path));

  await result[Symbol.asyncDispose]();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error
  await result[Symbol.asyncDispose]();
}

async function chdirDoesNotAffectRemoval() {
  const originalCwd = process.cwd();

  process.chdir(tmpdir.path);
  const first = await fsPromises.mkdtempDisposable('first.');
  const second = await fsPromises.mkdtempDisposable('second.');

  const fullFirstPath = path.join(tmpdir.path, first.path);
  const fullSecondPath = path.join(tmpdir.path, second.path);

  assert(fs.existsSync(fullFirstPath));
  assert(fs.existsSync(fullSecondPath));

  process.chdir(fullFirstPath);
  await second.remove();

  assert(!fs.existsSync(fullSecondPath));

  process.chdir(tmpdir.path);
  await first.remove();
  assert(!fs.existsSync(fullFirstPath));

  process.chdir(originalCwd);
}

async function errorsAreReThrown() {
  const base = await fsPromises.mkdtempDisposable(tmpdir.resolve('foo.'));

  if (common.isWindows) {
    // On Windows we can prevent removal by holding a file open
    const testFile = path.join(base.path, 'locked-file.txt');
    fs.writeFileSync(testFile, 'test');
    const fd = fs.openSync(testFile, 'r');

    await assert.rejects(base.remove(), /EBUSY|ENOTEMPTY|EPERM/);

    fs.closeSync(fd);
    fs.unlinkSync(testFile);

    // Removal works once file is closed
    await base.remove();
    assert(!fs.existsSync(base.path));
  } else {
    // On Unix we can prevent removal by making the parent directory read-only
    const child = await fsPromises.mkdtempDisposable(path.join(base.path, 'bar.'));

    const originalMode = fs.statSync(base.path).mode;
    fs.chmodSync(base.path, 0o444);

    await assert.rejects(child.remove(), /EACCES|EPERM/);

    fs.chmodSync(base.path, originalMode);

    // Removal works once permissions are reset
    await child.remove();
    assert(!fs.existsSync(child.path));

    await base.remove();
    assert(!fs.existsSync(base.path));
  }
}

(async () => {
  await basicUsage();
  await symbolAsyncDispose();
  await chdirDoesNotAffectRemoval();
  await errorsAreReThrown();
})().then(common.mustCall());
