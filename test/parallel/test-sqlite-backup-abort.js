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

  const controller = new AbortController();

  // Abort after a short delay
  const abortTimer = setTimeout(() => {
    controller.abort('Test cancellation');
  }, 10);

  try {
    await backup(sourceDb, destDbPath, {
      signal: controller.signal,
      rate: 1 // Very slow to increase chance of abort
    });

    // If we get here, the backup completed before abort
    clearTimeout(abortTimer);
    // This is acceptable - the backup might finish before abort triggers
  } catch (error) {
    clearTimeout(abortTimer);
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

  const controller = new AbortController();
  controller.abort('Already aborted');

  await assert.rejects(
    backup(sourceDb, destDbPath, { signal: controller.signal }),
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
      name: 'TypeError',
      message: /The "options\.signal" argument must be an AbortSignal/
    }
  );
});
