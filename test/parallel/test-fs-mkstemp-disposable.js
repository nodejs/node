'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');
const { isMainThread } = require('worker_threads');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Sync basic usage
{
  const result = fs.mkstempSync(tmpdir.resolve('sync-remove.'));

  assert.strictEqual(path.basename(result.path).length, 'sync-remove.XXXXXX'.length);
  assert.strictEqual(path.dirname(result.path), tmpdir.path);
  assert.strictEqual(typeof result.fd, 'number');
  assert.strictEqual(typeof result.remove, 'function');
  assert.strictEqual(typeof result[Symbol.dispose], 'function');
  assert(fs.existsSync(result.path));

  fs.writeSync(result.fd, 'hello');
  result.remove();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error (idempotent)
  result.remove();
}

// Sync usage with [Symbol.dispose]()
{
  const result = fs.mkstempSync(tmpdir.resolve('sync-dispose.'));

  assert(fs.existsSync(result.path));

  fs.writeSync(result.fd, 'hello');
  result[Symbol.dispose]();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error (idempotent)
  result[Symbol.dispose]();
}

// Sync when fd is closed manually prior to disposal
{
  const result = fs.mkstempSync(tmpdir.resolve('sync-manual-close.'));

  assert(fs.existsSync(result.path));
  fs.closeSync(result.fd);

  result.remove();
  assert(!fs.existsSync(result.path));
}

// Sync with Buffer prefix
{
  const result = fs.mkstempSync(Buffer.from(tmpdir.resolve('sync-buffer.')));

  assert(Buffer.isBuffer(result.path));
  assert(fs.existsSync(result.path.toString()));

  result.remove();
  assert(!fs.existsSync(result.path.toString()));
}

// Sync `chdir` does not affect removal
if (isMainThread) {
  const originalCwd = process.cwd();
  const otherDir = tmpdir.resolve('other-dir-sync');
  fs.mkdirSync(otherDir);

  process.chdir(tmpdir.path);
  const first = fs.mkstempSync('first.');
  const second = fs.mkstempSync('second.');

  const fullFirstPath = path.join(tmpdir.path, first.path);
  const fullSecondPath = path.join(tmpdir.path, second.path);

  assert(fs.existsSync(fullFirstPath));
  assert(fs.existsSync(fullSecondPath));

  process.chdir(otherDir);
  second.remove();

  assert(!fs.existsSync(fullSecondPath));

  process.chdir(tmpdir.path);
  first.remove();
  assert(!fs.existsSync(fullFirstPath));

  process.chdir(originalCwd);
}

// Callback basic usage
{
  fs.mkstemp(tmpdir.resolve('callback-remove.'), common.mustSucceed((result) => {
    assert.strictEqual(typeof result.fd, 'number');
    assert.strictEqual(typeof result.remove, 'function');
    assert.strictEqual(typeof result[Symbol.dispose], 'function');
    assert(fs.existsSync(result.path));

    fs.writeSync(result.fd, 'hello');
    result.remove();

    assert(!fs.existsSync(result.path));

    // Second removal does not throw error (idempotent)
    result.remove();
  }));

  fs.mkstemp(tmpdir.resolve('callback-dispose.'), common.mustSucceed((result) => {
    assert(fs.existsSync(result.path));

    fs.writeSync(result.fd, 'hello');
    result[Symbol.dispose]();

    assert(!fs.existsSync(result.path));

    result[Symbol.dispose]();
  }));

  fs.mkstemp(tmpdir.resolve('callback-manual-close.'), common.mustSucceed((result) => {
    assert(fs.existsSync(result.path));
    fs.closeSync(result.fd);

    result.remove();
    assert(!fs.existsSync(result.path));
  }));
}

// Promises basic usage
async function promisesBasicUsage() {
  const result = await fsPromises.mkstemp(tmpdir.resolve('promise-remove.'));

  assert.strictEqual(path.basename(result.path).length, 'promise-remove.XXXXXX'.length);
  assert.strictEqual(path.dirname(result.path), tmpdir.path);
  assert.ok(result.handle);
  assert.strictEqual(typeof result.remove, 'function');
  assert.strictEqual(typeof result[Symbol.asyncDispose], 'function');
  assert(fs.existsSync(result.path));

  await result.handle.writeFile('hello');
  await result.remove();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error (idempotent)
  await result.remove();
}

// Promises usage with [Symbol.asyncDispose]()
async function promisesSymbolAsyncDispose() {
  const result = await fsPromises.mkstemp(tmpdir.resolve('promise-dispose.'));

  assert(fs.existsSync(result.path));

  await result.handle.writeFile('hello');
  await result[Symbol.asyncDispose]();

  assert(!fs.existsSync(result.path));

  // Second removal does not throw error (idempotent)
  await result[Symbol.asyncDispose]();
}

// Promises when handle is closed manually prior to disposal
async function promisesManualClose() {
  const result = await fsPromises.mkstemp(tmpdir.resolve('promise-manual-close.'));

  assert(fs.existsSync(result.path));
  await result.handle.close();

  await result.remove();
  assert(!fs.existsSync(result.path));
}

// Promises with Buffer prefix
async function promisesBufferPrefix() {
  const result = await fsPromises.mkstemp(Buffer.from(tmpdir.resolve('promise-buffer.')));

  assert(Buffer.isBuffer(result.path));
  assert(fs.existsSync(result.path.toString()));

  await result.remove();
  assert(!fs.existsSync(result.path.toString()));
}

// Promises `chdir` does not affect removal
async function promisesChdirDoesNotAffectRemoval() {
  if (!isMainThread) return;

  const originalCwd = process.cwd();
  const otherDir = tmpdir.resolve('other-dir-promise');
  fs.mkdirSync(otherDir);

  process.chdir(tmpdir.path);
  const first = await fsPromises.mkstemp('first-p.');
  const second = await fsPromises.mkstemp('second-p.');

  const fullFirstPath = path.join(tmpdir.path, first.path);
  const fullSecondPath = path.join(tmpdir.path, second.path);

  assert(fs.existsSync(fullFirstPath));
  assert(fs.existsSync(fullSecondPath));

  process.chdir(otherDir);
  await second.remove();

  assert(!fs.existsSync(fullSecondPath));

  process.chdir(tmpdir.path);
  await first.remove();
  assert(!fs.existsSync(fullFirstPath));

  process.chdir(originalCwd);
}

// Errors from cleanup are thrown
// On Unix we can prevent removal by making the parent directory read-only
async function errorsAreReThrown() {
  if (common.isWindows || process.getuid() === 0) return;

  const subDir = tmpdir.resolve('readonly-dir');
  fs.mkdirSync(subDir);

  const file = fs.mkstempSync(path.join(subDir, 'child.'));
  fs.chmodSync(subDir, 0o444);

  assert.throws(() => {
    file.remove();
  }, /EACCES|EPERM/);

  fs.chmodSync(subDir, 0o777);
  file.remove();
  assert(!fs.existsSync(file.path));

  fs.rmdirSync(subDir);
}

(async () => {
  await promisesBasicUsage();
  await promisesSymbolAsyncDispose();
  await promisesManualClose();
  await promisesBufferPrefix();
  await promisesChdirDoesNotAffectRemoval();
  await errorsAreReThrown();
})().then(common.mustCall());
