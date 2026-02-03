'use strict';
// Flags: --expose-gc

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

const assert = require('assert');
const { DatabaseSync } = require('node:sqlite');
const { test, beforeEach } = require('node:test');

const db = new DatabaseSync(':memory:');
const sql = db.createTagStore(10);

beforeEach(() => {
  db.exec('DROP TABLE IF EXISTS foo');
  db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY, text TEXT)');
  sql.clear();
});

test('throws error if database is not open', () => {
  const db = new DatabaseSync(':memory:', { open: false });

  assert.throws(() => {
    db.createTagStore(10);
  }, {
    code: 'ERR_INVALID_STATE',
    message: 'database is not open'
  });
});

test('sql.run inserts data', () => {
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'bob'})`.changes, 1);
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'mac'})`.changes, 1);
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'alice'})`.changes, 1);

  const count = db.prepare('SELECT COUNT(*) as count FROM foo').get().count;
  assert.strictEqual(count, 3);
});

test('sql.get retrieves a single row', () => {
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'bob'})`.changes, 1);
  const first = sql.get`SELECT * FROM foo ORDER BY id ASC`;
  assert.ok(first);
  assert.strictEqual(first.text, 'bob');
  assert.strictEqual(first.id, 1);
  assert.strictEqual(Object.getPrototypeOf(first), null);
});

test('sql.all retrieves all rows', () => {
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'bob'})`.changes, 1);
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'mac'})`.changes, 1);
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'alice'})`.changes, 1);

  const all = sql.all`SELECT * FROM foo ORDER BY id ASC`;
  assert.strictEqual(Array.isArray(all), true);
  assert.strictEqual(all.length, 3);
  for (const row of all) {
    assert.strictEqual(Object.getPrototypeOf(row), null);
  }
  assert.deepStrictEqual(all.map((r) => r.text), ['bob', 'mac', 'alice']);
});

test('sql.iterate retrieves rows via iterator', () => {
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'bob'})`.changes, 1);
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'mac'})`.changes, 1);
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'alice'})`.changes, 1);

  const iter = sql.iterate`SELECT * FROM foo ORDER BY id ASC`;
  const iterRows = [];
  for (const row of iter) {
    assert.ok(row);
    assert.strictEqual(Object.getPrototypeOf(row), null);
    iterRows.push(row.text);
  }
  assert.deepStrictEqual(iterRows, ['bob', 'mac', 'alice']);
});

test('queries with no results', () => {
  const none = sql.get`SELECT * FROM foo WHERE text = ${'notfound'}`;
  assert.strictEqual(none, undefined);

  const empty = sql.all`SELECT * FROM foo WHERE text = ${'notfound'}`;
  assert.deepStrictEqual(empty, []);

  let count = 0;
  // eslint-disable-next-line no-unused-vars
  for (const _ of sql.iterate`SELECT * FROM foo WHERE text = ${'notfound'}`) {
    count++;
  }
  assert.strictEqual(count, 0);
});

test('TagStore capacity, size, and clear', () => {
  assert.strictEqual(sql.capacity, 10);
  assert.strictEqual(sql.size, 0);

  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'one'})`.changes, 1);
  assert.strictEqual(sql.size, 1);

  assert.ok(sql.get`SELECT * FROM foo WHERE text = ${'one'}`);
  assert.strictEqual(sql.size, 2);

  // Using the same template string shouldn't increase the size
  assert.strictEqual(sql.get`SELECT * FROM foo WHERE text = ${'two'}`, undefined);
  assert.strictEqual(sql.size, 2);

  assert.strictEqual(sql.all`SELECT * FROM foo`.length, 1);
  assert.strictEqual(sql.size, 3);

  sql.clear();
  assert.strictEqual(sql.size, 0);
  assert.strictEqual(sql.capacity, 10);
});

test('sql.db returns the associated DatabaseSync instance', () => {
  assert.strictEqual(sql.db, db);
});

test('sql error messages are descriptive', () => {
  assert.strictEqual(sql.run`INSERT INTO foo (text) VALUES (${'test'})`.changes, 1);

  // Test with non-existent column
  assert.throws(() => {
    const result = sql.get`SELECT nonexistent_column FROM foo`;
    assert.fail(`Expected error, got: ${JSON.stringify(result)}`);
  }, {
    code: 'ERR_SQLITE_ERROR',
    message: /no such column/i,
  });

  // Test with non-existent table
  assert.throws(() => {
    const result = sql.get`SELECT * FROM nonexistent_table`;
    assert.fail(`Expected error, got: ${JSON.stringify(result)}`);
  }, {
    code: 'ERR_SQLITE_ERROR',
    message: /no such table/i,
  });
});

test('a tag store keeps the database alive by itself', () => {
  const sql = new DatabaseSync(':memory:').createTagStore();

  sql.db.exec('CREATE TABLE test (data INTEGER)');

  global.gc();

  // eslint-disable-next-line no-unused-expressions
  sql.run`INSERT INTO test (data) VALUES (1)`;
});

test('tag store prevents circular reference leaks', async () => {
  const { gcUntil } = require('../common/gc');

  const before = process.memoryUsage().heapUsed;

  // Create many SQLTagStore + DatabaseSync pairs with circular references
  for (let i = 0; i < 1000; i++) {
    const sql = new DatabaseSync(':memory:').createTagStore();
    sql.db.exec('CREATE TABLE test (data INTEGER)');
    // eslint-disable-next-line no-void
    sql.db.setAuthorizer(() => void sql.db);
  }

  // GC until memory stabilizes or give up after 20 attempts
  await gcUntil('tag store leak check', () => {
    const after = process.memoryUsage().heapUsed;
    // Memory should not grow significantly (allow 50% margin for noise)
    return after < before * 1.5;
  }, 20);
});
