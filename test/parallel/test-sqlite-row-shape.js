// Flags: --allow-natives-syntax
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

const hasFastProperties = eval('(o) => %HasFastProperties(o)');
const haveSameMap = eval('(a, b) => %HaveSameMap(a, b)');

function assertSharedFastShape(t, rows) {
  t.assert.ok(rows.length >= 2);
  for (const row of rows) {
    t.assert.strictEqual(Object.getPrototypeOf(row), null);
    t.assert.ok(hasFastProperties(row));
    t.assert.ok(haveSameMap(rows[0], row));
  }
}

function createDatabase() {
  const db = new DatabaseSync(':memory:');
  db.exec('CREATE TABLE t (a INTEGER, b TEXT); ' +
          "INSERT INTO t VALUES (1, 'x'), (2, 'y'), (3, 'z');");
  return db;
}

suite('result rows share a fast map', () => {
  test('all()', (t) => {
    using db = createDatabase();
    using stmt = db.prepare('SELECT a, b FROM t');
    assertSharedFastShape(t, stmt.all());
  });

  test('get()', (t) => {
    using db = createDatabase();
    using stmt = db.prepare('SELECT a, b FROM t WHERE a = ?');
    assertSharedFastShape(t, [stmt.get(1), stmt.get(2)]);
  });

  test('iterate()', (t) => {
    using db = createDatabase();
    using stmt = db.prepare('SELECT a, b FROM t');
    assertSharedFastShape(t, [...stmt.iterate()]);
  });
});

test('rows with differing value types keep their values', (t) => {
  using db = new DatabaseSync(':memory:');
  db.exec('CREATE TABLE t (v); ' +
          "INSERT INTO t VALUES (NULL), (1), (1.5), ('s'), (x'01');");
  using stmt = db.prepare('SELECT v FROM t ORDER BY rowid');
  const expected = [
    { __proto__: null, v: null },
    { __proto__: null, v: 1 },
    { __proto__: null, v: 1.5 },
    { __proto__: null, v: 's' },
    { __proto__: null, v: new Uint8Array([1]) },
  ];
  t.assert.deepStrictEqual(stmt.all(), expected);
  t.assert.deepStrictEqual([...stmt.iterate()], expected);
  stmt.setReadBigInts(true);
  t.assert.deepStrictEqual(stmt.all()[1], { __proto__: null, v: 1n });
});

test('column names that cannot be template keys', (t) => {
  using db = new DatabaseSync(':memory:');
  const cases = [
    ['SELECT 1 AS "0"', { __proto__: null, 0: 1 }],
    ['SELECT 1 AS "4294967294"', { __proto__: null, 4294967294: 1 }],
    ['SELECT 1 AS a, 2 AS a', { __proto__: null, a: 2 }],
    ['SELECT 1 AS "café"', { __proto__: null, café: 1 }],
  ];
  for (const [sql, row] of cases) {
    using stmt = db.prepare(sql);
    t.assert.deepStrictEqual(stmt.get(), row);
    t.assert.deepStrictEqual(stmt.all(), [row]);
    t.assert.deepStrictEqual([...stmt.iterate()], [row]);
  }
});

test('names that look numeric but are not array indices', (t) => {
  using db = new DatabaseSync(':memory:');
  using stmt = db.prepare(
    'SELECT 1 AS "01", 2 AS "4294967295", 3 AS "-1", 4 AS "1.5"');
  const row = {
    __proto__: null, ['01']: 1, 4294967295: 2, ['-1']: 3, ['1.5']: 4,
  };
  t.assert.deepStrictEqual(stmt.get(), row);
  t.assert.deepStrictEqual([...stmt.iterate()], [row]);
});

test('iterate() picks up columns added by a re-prepare', (t) => {
  using db = new DatabaseSync(':memory:');
  db.exec("CREATE TABLE t (a); INSERT INTO t VALUES ('x');");
  using stmt = db.prepare('SELECT * FROM t');
  t.assert.deepStrictEqual([...stmt.iterate()], [{ __proto__: null, a: 'x' }]);
  db.exec("ALTER TABLE t ADD COLUMN b DEFAULT 'y';");
  t.assert.deepStrictEqual([...stmt.iterate()],
                           [{ __proto__: null, a: 'x', b: 'y' }]);
});
