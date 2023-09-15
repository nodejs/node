// Flags: --experimental-webstorage
'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { readdir, writeFile } = require('node:fs/promises');
const { beforeEach, test } = require('node:test');
const { spawnPromisified } = common;

if (!common.isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

beforeEach(() => {
  tmpdir.refresh();
  process.chdir(tmpdir.path);
});

test('disabled without --experimental-webstorage', async () => {
  for (const api of ['Storage', 'localStorage', 'sessionStorage']) {
    const cp = await spawnPromisified(process.execPath, ['-e', api]);

    assert.strictEqual(cp.code, 1);
    assert.strictEqual(cp.signal, null);
    assert.strictEqual(cp.stdout, '');
    assert(cp.stderr.includes(`ReferenceError: ${api} is not defined`));
  }
});

test('emits a warning when used', async () => {
  for (const api of ['Storage', 'localStorage', 'sessionStorage']) {
    const cp = await spawnPromisified(process.execPath, [
      '--experimental-webstorage', '-e', api,
    ]);

    assert.strictEqual(cp.code, 0);
    assert.strictEqual(cp.signal, null);
    assert.strictEqual(cp.stdout, '');
    assert.match(cp.stderr, /ExperimentalWarning: Web storage/);
  }
});

test('Storage instances cannot be created in userland', () => {
  assert.throws(() => {
    new globalThis.Storage();
  }, /Error: Illegal constructor/);
});

test('sessionStorage is not persisted', async () => {
  let cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-pe', 'sessionStorage.foo = "barbaz"',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);

  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-pe', 'sessionStorage.foo',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /undefined/);
  assert.strictEqual((await readdir(process.cwd())).length, 0);
});

test('localStorage is not persisted if it is unused', async () => {
  const cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-pe', 'localStorage === global.localStorage',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /true/);
  assert.strictEqual((await readdir(process.cwd())).length, 0);
});

test('localStorage is persisted if it is used', async () => {
  let cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-pe', 'localStorage.foo = "barbaz"',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);
  const entries = await readdir(process.cwd());
  assert.strictEqual(entries.length, 1);
  assert.match(entries[0], /node\.\d+\.localstorage/);

  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-pe', 'localStorage.foo',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);
});

test('localStorage is unique per main entrypoint', async () => {
  await writeFile('file1.js', 'localStorage.foo = 123;');
  await writeFile('file2.js', 'localStorage.foo = 789;');
  let cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', 'file1.js',
  ]);
  assert.strictEqual(cp.code, 0);
  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', 'file2.js',
  ]);
  assert.strictEqual(cp.code, 0);
  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-e', 'localStorage.foo = 999',
  ]);
  assert.strictEqual(cp.code, 0);
  // Two .js files and three .localstorage files.
  assert.strictEqual((await readdir(process.cwd())).length, 5);
  await writeFile('file1.js', 'console.log(localStorage.foo);');
  await writeFile('file2.js', 'console.log(localStorage.foo);');
  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', 'file1.js',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /123/);
  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', 'file2.js',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /789/);
  cp = await spawnPromisified(process.execPath, [
    '--experimental-webstorage', '-pe', 'localStorage.foo',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /999/);
});
