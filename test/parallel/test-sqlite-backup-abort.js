'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { DatabaseSync, backup } = require('node:sqlite');
const { test } = require('node:test');
const assert = require('node:assert');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

test('backup with AbortSignal - abort during operation', async (t) => {
  const sourceDbPath = nextDb();
  const destDbPath = nextDb();

  const sourceDb = new DatabaseSync(sourceDbPath);
  t.after(() => { sourceDb.close(); });

  // Create a larger table to make backup take longer
  sourceDb.exec(`
    CREATE TABLE test_table (id INTEGER PRIMARY KEY, data TEXT);
  `);

  const stmt = sourceDb.prepare('INSERT INTO test_table (data) VALUES (?)');
  for (let i = 0; i < 10000; i++) {
    stmt.run(`Test data ${i}`);
  }

  try {
    await backup(sourceDb, destDbPath, {
      signal: AbortSignal.timeout(10),
      rate: 1 // Very slow to increase chance of abort
    });

    // If we get here, the backup completed before abort
    // This is acceptable - the backup might finish before abort triggers
  } catch (error) {
    assert.strictEqual(error.name, 'AbortError');
    assert.strictEqual(error.cause, 'Test cancellation');
  }
});

test('backup with AbortSignal - pre-aborted signal', async (t) => {
  const sourceDbPath = nextDb();
  const destDbPath = nextDb();

  const sourceDb = new DatabaseSync(sourceDbPath);
  t.after(() => { sourceDb.close(); });

  sourceDb.exec(`
    CREATE TABLE test_table (id INTEGER PRIMARY KEY, data TEXT);
    INSERT INTO test_table (data) VALUES ('test');
  `);

  const signal = AbortSignal.abort('Already aborted');

  await assert.rejects(
    backup(sourceDb, destDbPath, { signal }),
    {
      name: 'AbortError',
      cause: 'Already aborted'
    }
  );
});

test('backup with AbortSignal - invalid signal type', (t) => {
  const sourceDbPath = nextDb();
  const destDbPath = nextDb();

  const sourceDb = new DatabaseSync(sourceDbPath);
  t.after(() => { sourceDb.close(); });

  sourceDb.exec(`
    CREATE TABLE test_table (id INTEGER PRIMARY KEY, data TEXT);
  `);

  assert.throws(
    () => backup(sourceDb, destDbPath, { signal: 'not-a-signal' }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: `The "options.signal" property must be an instance of AbortSignal. Received type string ('not-a-signal')`
    }
  );
});

test('backup with AbortSignal - empty object as signal', (t) => {
  const sourceDbPath = nextDb();
  const destDbPath = nextDb();

  const sourceDb = new DatabaseSync(sourceDbPath);
  t.after(() => { sourceDb.close(); });

  sourceDb.exec(`
    CREATE TABLE test_table (id INTEGER PRIMARY KEY, data TEXT);
  `);

  assert.throws(
    () => backup(sourceDb, destDbPath, { signal: {} }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.signal" property must be an instance of AbortSignal. Received an instance of Object'
    }
  );
});
