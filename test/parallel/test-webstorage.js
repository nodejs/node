'use strict';

const {
  isLinux, isMacOS, skipIfSQLiteMissing, spawnPromisified,
} = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { join } = require('node:path');
const { readdir } = require('node:fs/promises');
const { DatabaseSync } = require('node:sqlite');
const { test, describe } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextLocalStorage() {
  return join(tmpdir.path, `${++cnt}.localstorage`);
}

// The tests below assert on which .localstorage files exist, so malformed
// fixtures are named so as not to be counted among them.
function nextMalformedLocalStorage() {
  return join(tmpdir.path, `malformed-${++cnt}.db`);
}

async function localStorageFiles() {
  return (await readdir(tmpdir.path)).filter((f) => f.endsWith('.localstorage'));
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

test('calling "length" getter on invalid this throws', async () => {
  assert.throws(() => Storage.prototype.length, TypeError);
  const { get } = Object.getOwnPropertyDescriptor(Storage.prototype, 'length');
  for (const thisArg of [null, undefined, 1n, -0, NaN, true, false, '', [], {}, Symbol()]) {
    assert.throws(() => get.call(thisArg), TypeError);
  }
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
  assert.deepStrictEqual(await localStorageFiles(), []);
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
  assert.deepStrictEqual(await localStorageFiles(), []);
});

test('localStorage is persisted if it is used', async () => {
  const localStorageFile = nextLocalStorage();
  let cp = await spawnPromisified(process.execPath, [
    '--localstorage-file', localStorageFile,
    '-pe', 'localStorage.foo = "barbaz"',
  ]);
  assert.strictEqual(cp.code, 0);
  assert.match(cp.stdout, /barbaz/);
  const entries = await localStorageFiles();
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

describe('a malformed localStorage file throws instead of aborting', () => {
  // Node's own tables are STRICT, so it cannot store a wrong-typed value
  // itself. But they are created with IF NOT EXISTS, so a file that already
  // contains tables of those names is adopted as-is. Declare the same schema
  // without STRICT: BLOB columns have no affinity, so TEXT stays TEXT.
  function malformedLocalStorage(fill) {
    const file = nextMalformedLocalStorage();
    const db = new DatabaseSync(file);
    db.exec(`
      CREATE TABLE nodejs_webstorage(
        key BLOB NOT NULL, value BLOB NOT NULL, PRIMARY KEY(key)
      );
      CREATE TABLE nodejs_webstorage_state(
        max_size INTEGER NOT NULL DEFAULT 10485760,
        total_size INTEGER NOT NULL,
        schema_version INTEGER NOT NULL DEFAULT 1,
        single_row_ INTEGER NOT NULL DEFAULT 1 CHECK(single_row_ = 1),
        PRIMARY KEY(single_row_)
      );
    `);
    fill({
      insert: (key, value) => db.prepare(
        'INSERT INTO nodejs_webstorage (key, value) VALUES (?, ?)',
      ).run(key, value),
      setSchemaVersion: (schemaVersion) => db.prepare(
        'INSERT INTO nodejs_webstorage_state (total_size, schema_version)' +
        ' VALUES (0, ?)',
      ).run(schemaVersion),
    });
    db.close();
    return file;
  }

  // Keys are stored UTF-16LE, so a real key is needed for lookups to match.
  const utf16 = (str) => Buffer.from(str, 'utf16le');

  for (const [name, fill, expression, detail] of [
    [
      'a text schema_version',
      ({ setSchemaVersion }) => setSchemaVersion('one'),
      'localStorage.length',
      'expected schema_version to be an integer',
    ],
    [
      'a text key read by key()',
      ({ insert, setSchemaVersion }) => {
        insert('greeting', utf16('hello'));
        setSchemaVersion(1);
      },
      'localStorage.key(0)',
      'expected key to be a blob',
    ],
    [
      'a text key read by enumeration',
      ({ insert, setSchemaVersion }) => {
        insert('greeting', utf16('hello'));
        setSchemaVersion(1);
      },
      'Object.keys(localStorage)',
      'expected key to be a blob',
    ],
    [
      'a text value',
      ({ insert, setSchemaVersion }) => {
        insert(utf16('greeting'), 'hello');
        setSchemaVersion(1);
      },
      "localStorage.getItem('greeting')",
      'expected value to be a blob',
    ],
  ]) {
    test(`${name}, via ${expression}`, async () => {
      const cp = await spawnPromisified(process.execPath, [
        '--localstorage-file', malformedLocalStorage(fill),
        '-e', expression,
      ]);

      assert.strictEqual(cp.code, 1);
      assert.strictEqual(cp.signal, null);
      assert(cp.stderr.includes(
        `Error: localStorage database is malformed: ${detail}`,
      ));
      assert(cp.stderr.includes("code: 'ERR_INVALID_STATE'"));
    });
  }
});

test('a malformed localStorage file does not leak connections', {
  // Counting the process's own descriptors needs a /proc/self/fd or /dev/fd
  // that lists all of them. AIX and IBM i expose only 0, 1 and 2 there, which
  // would make the count constant and the test vacuous.
  skip: (!isLinux && !isMacOS) && 'cannot enumerate open descriptors',
}, async () => {
  const file = nextMalformedLocalStorage();
  const db = new DatabaseSync(file);
  db.exec(`
    CREATE TABLE nodejs_webstorage_state(
      max_size INTEGER NOT NULL DEFAULT 10485760,
      total_size INTEGER NOT NULL,
      schema_version INTEGER NOT NULL DEFAULT 1,
      single_row_ INTEGER NOT NULL DEFAULT 1 CHECK(single_row_ = 1),
      PRIMARY KEY(single_row_)
    );
  `);
  db.prepare('INSERT INTO nodejs_webstorage_state (total_size, schema_version)' +
    ' VALUES (0, ?)').run('one');
  db.close();

  // A failed open used to leave its sqlite3* behind, two descriptors at a time,
  // so repeated access exhausted the descriptor limit and degraded the error
  // into a misleading "unable to open database file".
  const cp = await spawnPromisified(process.execPath, [
    '--localstorage-file', file,
    '-e', `
    const assert = require('assert');
    const { readdirSync } = require('fs');
    const fdDir = process.platform === 'linux' ? '/proc/self/fd' : '/dev/fd';
    const openDescriptors = () => readdirSync(fdDir).length;
    const attempt = () => assert.throws(() => localStorage.length, {
      code: 'ERR_INVALID_STATE',
      message: /expected schema_version to be an integer/,
    });

    attempt();
    const before = openDescriptors();
    for (let i = 0; i < 200; i++) attempt();
    const leaked = openDescriptors() - before;
    assert.ok(leaked < 20, 'leaked ' + leaked + ' descriptors');
    `,
  ]);
  assert.strictEqual(cp.code, 0, cp.stderr);
  assert.strictEqual(cp.stdout, '');
});
