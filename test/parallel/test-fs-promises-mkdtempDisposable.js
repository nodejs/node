'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');
const { isMainThread } = require('worker_threads');

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
  // Can't use chdir in workers
  if (!isMainThread) return;

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

async function bufferPaths() {
  const prefix = tmpdir.resolve('foo.');
  for (const result of [
    await fsPromises.mkdtempDisposable(Buffer.from(prefix)),
    await fsPromises.mkdtempDisposable(prefix, { encoding: 'buffer' }),
  ]) {
    assert(Buffer.isBuffer(result.path));
    assert.strictEqual(path.dirname(result.path.toString()), tmpdir.path);
    assert(fs.existsSync(result.path));

    await result.remove();

    assert(!fs.existsSync(result.path));
  }
}

async function chdirDoesNotAffectRemovalOfBufferPath() {
  // Can't use chdir in workers
  if (!isMainThread) return;

  const originalCwd = process.cwd();

  process.chdir(tmpdir.path);
  const result = await fsPromises.mkdtempDisposable(Buffer.from('buffer.'));
  const fullPath = path.join(tmpdir.path, result.path.toString());

  assert(fs.existsSync(fullPath));

  process.chdir(originalCwd);
  await result.remove();

  assert(!fs.existsSync(fullPath));
}

async function nonUtf8BufferPath() {
  // macOS rejects such file names, and Windows converts paths from UTF-8
  if (!common.isLinux) return;
  const prefix = Buffer.concat([Buffer.from(tmpdir.resolve('foo')), Buffer.from([0xff, 0x2e])]);
  const result = await fsPromises.mkdtempDisposable(prefix);

  assert.deepStrictEqual(result.path.subarray(0, prefix.length), prefix);
  assert(fs.existsSync(result.path));

  await result.remove();

  assert(!fs.existsSync(result.path));
}

async function relativeBufferPathInNonAsciiCwd() {
  // Can't use chdir in workers
  if (!isMainThread) return;

  const originalCwd = process.cwd();
  const nonAscii = fs.mkdtempSync(path.join(tmpdir.path, '\u7528\u6237-'));

  process.chdir(nonAscii);
  const result = await fsPromises.mkdtempDisposable(Buffer.from('buffer.'));
  const fullPath = path.join(nonAscii, result.path.toString());

  assert(fs.existsSync(fullPath));

  process.chdir(originalCwd);
  await result.remove();

  assert(!fs.existsSync(fullPath));
  fs.rmSync(nonAscii, { recursive: true });
}

async function symlinkDotDotKeepsCreatedDirectory() {
  // Windows normalizes `..` before following the symlink.
  if (common.isWindows) return;

  const outside = fs.mkdtempSync(path.join(tmpdir.path, 'outside-'));
  const parent = fs.mkdtempSync(path.join(tmpdir.path, 'parent-'));
  const link = path.join(parent, 'link');
  fs.symlinkSync(outside, link);

  const prefix = Buffer.from(`${link}/../foo.`);
  const result = await fsPromises.mkdtempDisposable(prefix);
  const baseName = path.basename(result.path.toString());
  const createdPath = path.join(path.dirname(outside), baseName);

  assert(fs.existsSync(createdPath));
  assert(!fs.existsSync(path.join(parent, baseName)));

  await result.remove();

  assert(!fs.existsSync(createdPath));
  fs.rmSync(outside, { recursive: true });
  fs.rmSync(parent, { recursive: true });
}

async function errorsAreReThrown() {
  // It is difficult to arrange for rmdir to fail on windows
  if (common.isWindows || process.getuid() === 0) return;
  const base = await fsPromises.mkdtempDisposable(tmpdir.resolve('foo.'));

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

(async () => {
  await basicUsage();
  await symbolAsyncDispose();
  await chdirDoesNotAffectRemoval();
  await bufferPaths();
  await chdirDoesNotAffectRemovalOfBufferPath();
  await nonUtf8BufferPath();
  await relativeBufferPathInNonAsciiCwd();
  await symlinkDotDotKeepsCreatedDirectory();
  await errorsAreReThrown();
})().then(common.mustCall());
