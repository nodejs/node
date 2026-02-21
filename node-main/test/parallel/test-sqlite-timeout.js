'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { test } = require('node:test');
const { once } = require('node:events');
const { Worker } = require('node:worker_threads');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

test('waits to acquire lock', async (t) => {
  const DB_PATH = nextDb();
  const conn = new DatabaseSync(DB_PATH);
  t.after(() => {
    try {
      conn.close();
    } catch {
      // Ignore.
    }
  });

  conn.exec('CREATE TABLE IF NOT EXISTS data (value TEXT)');
  conn.exec('BEGIN EXCLUSIVE;');
  const worker = new Worker(`
    'use strict';
    const { DatabaseSync } = require('node:sqlite');
    const { workerData } = require('node:worker_threads');
    const conn = new DatabaseSync(workerData.database, { timeout: 30000 });
    conn.exec('SELECT * FROM data');
    conn.close();
  `, {
    eval: true,
    workerData: {
      database: DB_PATH,
    }
  });
  await once(worker, 'online');
  conn.exec('COMMIT;');
  await once(worker, 'exit');
});

test('throws if the lock cannot be acquired before timeout', (t) => {
  const DB_PATH = nextDb();
  const conn1 = new DatabaseSync(DB_PATH);
  t.after(() => {
    try {
      conn1.close();
    } catch {
      // Ignore.
    }
  });
  const conn2 = new DatabaseSync(DB_PATH, { timeout: 1 });
  t.after(() => {
    try {
      conn2.close();
    } catch {
      // Ignore.
    }
  });

  conn1.exec('CREATE TABLE IF NOT EXISTS data (value TEXT)');
  conn1.exec('PRAGMA locking_mode = EXCLUSIVE; BEGIN EXCLUSIVE;');
  t.assert.throws(() => {
    conn2.exec('SELECT * FROM data');
  }, /database is locked/);
});
