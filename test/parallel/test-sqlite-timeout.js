'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const { path: fixturesPath } = require('../common/fixtures');
const { spawn } = require('node:child_process');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

test('connection can perform queries when lock is released before the timeout', (t, done) => {
  const databasePath = nextDb();
  const conn = new DatabaseSync(databasePath, { timeout: 1100 });
  t.after(() => { conn.close(); });

  conn.exec('CREATE TABLE data (key INTEGER PRIMARY KEY, value TEXT)');

  // Spawns a child process to locks the database for 1 second
  const child = spawn('./node', [fixturesPath('sqlite/lock-db.js'), databasePath], {
    stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
  });

  child.on('message', (msg) => {
    if (msg === 'locked') {
      conn.exec('INSERT INTO data (key, value) VALUES (?, ?)', [1, 'value-1']);

      setTimeout(() => {
        done();
      }, 1100);
    }
  });
});

test('trhows if lock is holden longer than the provided timeout', (t, done) => {
  const databasePath = nextDb();
  const conn = new DatabaseSync(databasePath, { timeout: 800 });
  t.after(() => { conn.close(); });

  conn.exec('CREATE TABLE data (key INTEGER PRIMARY KEY, value TEXT)');

  // Spawns a child process to locks the database for 1 second
  const child = spawn('./node', [fixturesPath('sqlite/lock-db.js'), databasePath], {
    stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
  });

  child.on('message', (msg) => {
    if (msg === 'locked') {
      t.assert.throws(() => {
        conn.exec('INSERT INTO data (key, value) VALUES (?, ?)', [1, 'value-1']);
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: 'database is locked',
      });
    }
  });

  child.on('exit', (code) => {
    if (code === 0) {
      done();
    }
  });
});
