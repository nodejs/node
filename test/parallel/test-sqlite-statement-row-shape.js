// Flags: --allow-natives-syntax
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { Database } = require('node:sqlite');
const { suite, test } = require('node:test');

function assertSharedShape(rows) {
  for (const row of rows) {
    assert.strictEqual(Object.getPrototypeOf(row), null);
    assert(eval('%HasFastProperties(row)'));
    assert(eval('%HaveSameMap(row, rows[0])'));
  }
}

suite('result row objects', () => {
  const db = new Database(':memory:');
  db.exec('CREATE TABLE t (a INTEGER, b TEXT, c REAL)');
  const insert = db.prepare('INSERT INTO t VALUES (?, ?, ?)');
  insert.run(1, 'x', 1.5);
  insert.run(2, 'y', 2.5);
  insert.run(3, null, null);

  test('all() rows share a fast map', () => {
    const rows = db.prepare('SELECT * FROM t ORDER BY a').all();
    assert.deepStrictEqual(rows, [
      { __proto__: null, a: 1, b: 'x', c: 1.5 },
      { __proto__: null, a: 2, b: 'y', c: 2.5 },
      { __proto__: null, a: 3, b: null, c: null },
    ]);
    assertSharedShape(rows);
  });

  test('get() rows share a fast map', () => {
    const stmt = db.prepare('SELECT * FROM t WHERE a = ?');
    assertSharedShape([stmt.get(1), stmt.get(2), stmt.get(3)]);
  });

  test('iterate() rows share a fast map', () => {
    assertSharedShape(db.prepare('SELECT * FROM t ORDER BY a').iterate().toArray());
  });

  test('a re-prepared statement picks up the new columns', () => {
    using db = new Database(':memory:');
    db.exec('CREATE TABLE s (a INTEGER)');
    db.exec('INSERT INTO s VALUES (1)');
    const stmt = db.prepare('SELECT * FROM s');
    assert.deepStrictEqual(stmt.get(), { __proto__: null, a: 1 });
    db.exec('ALTER TABLE s ADD COLUMN b INTEGER DEFAULT 2');
    assert.deepStrictEqual(stmt.get(), { __proto__: null, a: 1, b: 2 });
  });

  test('array-index column names', () => {
    const stmt = db.prepare('SELECT 1 AS "0", 2 AS b');
    assert.deepStrictEqual(stmt.get(), { __proto__: null, 0: 1, b: 2 });
    assert.deepStrictEqual(stmt.all(), [{ __proto__: null, 0: 1, b: 2 }]);
  });

  test('duplicate column names keep the last value', () => {
    const stmt = db.prepare('SELECT 1 AS a, 2 AS a');
    assert.deepStrictEqual(stmt.get(), { __proto__: null, a: 2 });
    assert.deepStrictEqual(stmt.iterate().toArray(),
                           [{ __proto__: null, a: 2 }]);
  });

  test('non-ASCII column names', () => {
    const row = db.prepare('SELECT 1 AS "café", 2 AS "名前"').get();
    assert.deepStrictEqual(row, { __proto__: null, café: 1, 名前: 2 });
  });

  test('rows wider than the template limit', () => {
    const cols = Array.from({ length: 100 }, (_, i) => `${i} AS c${i}`);
    const row = db.prepare(`SELECT ${cols.join(', ')}`).get();
    const expected = { __proto__: null };
    for (let i = 0; i < 100; i++) expected[`c${i}`] = i;
    assert.deepStrictEqual(row, expected);
  });
});
