'use strict';

const common = require('../common');
common.skipIfSQLiteMissing();

if (common.isWindows) {
  common.skip('SQLite on Windows uses HANDLEs, not fds');
}

const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawnPromisified } = common;
const { writeFileSync } = require('node:fs');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { test } = require('node:test');

tmpdir.refresh();

// Regression tests for https://github.com/nodejs/node/issues/64640. Failed
// localStorage initialisation leaked the SQLite connection opened by
// sqlite3_open(), which usually returns a database handle even on failure.
// Each retry then leaked one more file descriptor.
async function assertFailedInitDoesNotLeak(localStorageFile, message) {
  const cp = await spawnPromisified(process.execPath, [
    '--localstorage-file', localStorageFile,
    '-e', `
    const assert = require('node:assert');
    const { openSync, closeSync } = require('node:fs');
    // The lowest fd available to a new file. If a failed localStorage
    // initialisation leaks its connection, this number grows.
    const probeFd = () => {
      const fd = openSync(process.execPath, 'r');
      closeSync(fd);
      return fd;
    };
    const expected = { code: 'ERR_INVALID_STATE', message: ${message} };
    // Warm up lazily initialised resources before sampling the fd space.
    assert.throws(() => localStorage.length, expected);
    const before = probeFd();
    for (let i = 0; i < 15; i++) {
      assert.throws(() => localStorage.length, expected);
    }
    assert.strictEqual(probeFd(), before);
    `,
  ]);

  assert.strictEqual(cp.stderr, '');
  assert.strictEqual(cp.code, 0);
  assert.strictEqual(cp.signal, null);
}

test('corrupt non-SQLite file does not leak fds', async () => {
  const file = join(tmpdir.path, 'corrupt.localstorage');
  writeFileSync(file, 'not a sqlite database '.repeat(10));
  await assertFailedInitDoesNotLeak(file, '/not a database/');
});

test('unopenable database path does not leak fds', async () => {
  const file = join(tmpdir.path, 'missing-dir', 'db.localstorage');
  await assertFailedInitDoesNotLeak(file, '/unable to open database file/');
});

test('newer schema version does not leak fds', async () => {
  const file = join(tmpdir.path, 'newer-schema.localstorage');
  const db = new DatabaseSync(file);
  db.exec(`CREATE TABLE nodejs_webstorage_state(
    max_size INTEGER NOT NULL DEFAULT 10485760,
    total_size INTEGER NOT NULL,
    schema_version INTEGER NOT NULL DEFAULT 0,
    single_row_ INTEGER NOT NULL DEFAULT 1 CHECK(single_row_ = 1),
    PRIMARY KEY(single_row_)
  ) STRICT;
  INSERT INTO nodejs_webstorage_state (total_size, schema_version)
    VALUES (0, 99);`);
  db.close();
  await assertFailedInitDoesNotLeak(file, '/newer version of Node\\.js/');
});

test('empty state table does not leak fds', async () => {
  const file = join(tmpdir.path, 'zero-rows.localstorage');
  const db = new DatabaseSync(file);
  // The extra NOT NULL column makes the initialisation script's
  // INSERT OR IGNORE skip silently, leaving the state table empty, so
  // Open() fails while a prepared statement is still live.
  db.exec(`CREATE TABLE nodejs_webstorage_state(
    max_size INTEGER NOT NULL DEFAULT 10485760,
    total_size INTEGER NOT NULL,
    schema_version INTEGER NOT NULL DEFAULT 0,
    single_row_ INTEGER NOT NULL DEFAULT 1 CHECK(single_row_ = 1),
    extra INTEGER NOT NULL,
    PRIMARY KEY(single_row_)
  ) STRICT;`);
  db.close();
  await assertFailedInitDoesNotLeak(file, '/no more rows available/');
});

test('non-integer schema version throws instead of aborting', async () => {
  const file = join(tmpdir.path, 'text-schema.localstorage');
  const db = new DatabaseSync(file);
  db.exec(`CREATE TABLE nodejs_webstorage_state(
    max_size INTEGER NOT NULL DEFAULT 10485760,
    total_size INTEGER NOT NULL,
    schema_version TEXT NOT NULL,
    single_row_ INTEGER NOT NULL DEFAULT 1 CHECK(single_row_ = 1),
    PRIMARY KEY(single_row_)
  ) STRICT;
  INSERT INTO nodejs_webstorage_state (total_size, schema_version)
    VALUES (0, 'pwned');`);
  db.close();
  await assertFailedInitDoesNotLeak(
    file, '/schema version is not an integer/');
});
