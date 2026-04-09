'use strict';

const { skipIfSQLiteMissing, spawnPromisified } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { join } = require('node:path');
const { readdir } = require('node:fs/promises');
const { test, describe } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextLocalStorage() {
  return join(tmpdir.path, `${++cnt}.localstorage`);
}

test('Storage instances cannot be created in userland', async () => {
  const cp = await spawnPromisified(process.execPath, [
    '-e', 'new globalThis.Storage()',
  ]);

  assert.strictEqual(cp.code, 1);
  assert.strictEqual(cp.signal, null);
  assert.strictEqual(cp.stdout, '');
  assert.match(cp.stderr, /Error: Illegal constructor/);
});

test('sessionStorage is not persisted', async () => {
  let cp = await spawnPromisified(process.execPath, [
    '-pe', 'sessionStorage.foo = "barbaz"',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);

  cp = await spawnPromisified(process.execPath, [
    '-pe', 'sessionStorage.foo',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /undefined/);
  assert.strictEqual((await readdir(tmpdir.path)).length, 0);
});

test('localStorage returns undefined and warns without --localstorage-file', async () => {
  const cp = await spawnPromisified(process.execPath, [
    '-pe', 'localStorage',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.strictEqual(cp.signal, null);
  assert.match(cp.stdout, /undefined/);
  assert.match(cp.stderr, /ExperimentalWarning:.*localStorage is not available/);
});

test('localStorage is not enumerable without --localstorage-file', async () => {
  const cp = await spawnPromisified(process.execPath, [
    '-pe', 'Object.keys(globalThis).includes("localStorage")',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /false/);
});

test('localStorage is not persisted if it is unused', async () => {
  const cp = await spawnPromisified(process.execPath, [
    '--localstorage-file', nextLocalStorage(),
    '-pe', 'localStorage === globalThis.localStorage',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /true/);
  assert.strictEqual((await readdir(tmpdir.path)).length, 0);
});

test('localStorage is persisted if it is used', async () => {
  const localStorageFile = nextLocalStorage();
  let cp = await spawnPromisified(process.execPath, [
    '--localstorage-file', localStorageFile,
    '-pe', 'localStorage.foo = "barbaz"',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);
  const entries = await readdir(tmpdir.path);
  assert.strictEqual(entries.length, 1);
  assert.match(entries[0], /\d+\.localstorage/);

  cp = await spawnPromisified(process.execPath, [
    '--localstorage-file', localStorageFile,
    '-pe', 'localStorage.foo',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);
});


describe('webstorage quota for localStorage and sessionStorage', () => {
  const MAX_STORAGE_SIZE = 10 * 1024 * 1024;

  for (const storage of ['localStorage', 'sessionStorage']) {
    test(`${storage} can store a max of 10 MB quota`, async () => {
      const args = [];
      if (storage === 'localStorage') {
        args.push('--localstorage-file', nextLocalStorage());
      }
      // Each character is 2 bytes
      args.push('-e', `
      const assert = require('assert');
      ${storage}['a'.repeat(${MAX_STORAGE_SIZE} / 2)] = '';
      assert.throws(
        () => { ${storage}.anything = 'should fail'; },
        (err) => {
          assert.strictEqual(err.name, 'QuotaExceededError');
          assert.strictEqual(err.code, 22);
          assert(err instanceof DOMException);
          assert(err instanceof QuotaExceededError);
          assert.strictEqual(err.quota, null);
          assert.strictEqual(err.requested, null);
          return true;
        },
      );
      `);
      const cp = await spawnPromisified(process.execPath, args);
      assert.strictEqual(cp.code, 0);
    });
  }
});

test('disabled with --no-webstorage', async () => {
  for (const api of ['Storage', 'localStorage', 'sessionStorage']) {
    const cp = await spawnPromisified(process.execPath, [
      '--no-webstorage',
      '--localstorage-file',
      './test/fixtures/localstoragefile-global-test',
      '-e',
      api,
    ]);

    assert.strictEqual(cp.code, 1);
    assert.strictEqual(cp.signal, null);
    assert.strictEqual(cp.stdout, '');
    assert(cp.stderr.includes(`ReferenceError: ${api} is not defined`));
  }
});
