'use strict';
require('../common');
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
