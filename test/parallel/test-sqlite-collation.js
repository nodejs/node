// Flags: --expose-gc
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { Database } = require('node:sqlite');
const { suite, test } = require('node:test');

function reverse(a, b) {
  return a < b ? 1 : a > b ? -1 : 0;
}

function names(db, sql) {
  return db.prepare(sql).all().map((row) => row.name);
}

suite('Database.prototype.createCollation()', () => {
  suite('input validation', () => {
    const db = new Database(':memory:');

    test('throws if name is not a string', () => {
      assert.throws(() => {
        db.createCollation();
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "name" argument must be a string/,
      });
    });

    test('throws if compare is not a function', () => {
      assert.throws(() => {
        db.createCollation('foo', 'bar');
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "compare" argument must be a function/,
      });
    });

    test('throws if the database is not open', () => {
      const closed = new Database(':memory:', { open: false });
      assert.throws(() => {
        closed.createCollation('foo', reverse);
      }, {
        code: 'ERR_INVALID_STATE',
        message: /database is not open/,
      });
    });
  });

  test('is used by ORDER BY', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('b'), ('c'), ('a');`);
    assert.strictEqual(db.createCollation('reverse', reverse), undefined);
    assert.deepStrictEqual(
      names(db, 'SELECT name FROM t ORDER BY name COLLATE reverse'),
      ['c', 'b', 'a']);
  });

  test('is used by comparisons', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.createCollation('nocase_js', (a, b) => {
      return a.toLowerCase().localeCompare(b.toLowerCase());
    });
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('Ágætis'), ('zebra'), ('ZEBRA');`);
    assert.deepStrictEqual(
      names(db, "SELECT name FROM t WHERE name = 'Zebra' COLLATE nocase_js"),
      ['zebra', 'ZEBRA']);
  });

  test('receives strings and sorts Unicode text', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    const seen = new Set();
    db.createCollation('locale', (a, b) => {
      seen.add(typeof a).add(typeof b);
      return a.localeCompare(b, 'en');
    });
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('Zebra'), ('Ágætis'), (''), ('apple');`);
    assert.deepStrictEqual(
      names(db, 'SELECT name FROM t ORDER BY name COLLATE locale'),
      ['', 'Ágætis', 'apple', 'Zebra']);
    assert.deepStrictEqual([...seen], ['string']);
  });

  test('makes tables declared with the collation readable', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.createCollation('unicase', (a, b) => {
      return a.toLowerCase().localeCompare(b.toLowerCase());
    });
    db.exec(`CREATE TABLE tags (
               name TEXT PRIMARY KEY COLLATE unicase
             ) WITHOUT ROWID;
             INSERT INTO tags VALUES ('b'), ('A'), ('c');`);
    assert.throws(() => {
      db.exec("INSERT INTO tags VALUES ('a')");
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /UNIQUE constraint failed/,
    });
    assert.deepStrictEqual(names(db, 'SELECT name FROM tags'), ['A', 'b', 'c']);

    // Without the collation, the table cannot be read.
    const other = new Database(':memory:');
    t.after(() => other.close());
    other.deserialize(db.serialize());
    assert.throws(() => {
      other.prepare('SELECT * FROM tags').all();
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /no such collation sequence: unicase|no query solution/,
    });
  });

  test('converts the result like Array.prototype.sort()', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('b'), ('c'), ('a');`);
    const results = [
      [-0.5, 0.5],
      [-Infinity, Infinity],
      ['-1', '1'],
      [{ valueOf: () => -1 }, { valueOf: () => 1 }],
      [-1n, 1n, TypeError],
    ];
    for (const [less, greater, error] of results) {
      db.createCollation('custom', (a, b) => (a < b ? less : a > b ? greater : 0));
      const sql = 'SELECT name FROM t ORDER BY name COLLATE custom';
      if (error) {
        assert.throws(() => names(db, sql), error);
      } else {
        assert.deepStrictEqual(names(db, sql), ['a', 'b', 'c']);
      }
    }

    // NaN, undefined, and null mean the values are equal.
    for (const value of [NaN, undefined, null]) {
      db.createCollation('custom', () => value);
      assert.deepStrictEqual(
        names(db, "SELECT name FROM t WHERE name = 'x' COLLATE custom"),
        ['b', 'c', 'a']);
    }
  });

  test('can be replaced', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('b'), ('c'), ('a');`);
    const sql = 'SELECT name FROM t ORDER BY name COLLATE c';
    db.createCollation('c', reverse);
    assert.deepStrictEqual(names(db, sql), ['c', 'b', 'a']);
    db.createCollation('C', (a, b) => -reverse(a, b));
    assert.deepStrictEqual(names(db, sql), ['a', 'b', 'c']);
  });

  test('cannot be replaced while a statement is running', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.createCollation('c', reverse);
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('b'), ('a');`);
    const iterator = db.prepare('SELECT name FROM t ORDER BY name COLLATE c')
      .iterate();
    assert.strictEqual(iterator.next().value.name, 'b');
    assert.throws(() => {
      db.createCollation('c', (a, b) => -reverse(a, b));
    }, {
      code: 'ERR_SQLITE_ERROR',
      errcode: 5, // SQLITE_BUSY
    });
    // The original collation is still in use.
    assert.strictEqual(iterator.next().value.name, 'a');
    assert.strictEqual(iterator.next().done, true);
  });

  suite('errors thrown by compare', () => {
    function setup(t, compare) {
      const db = new Database(':memory:');
      t.after(() => db.close());
      db.exec('CREATE TABLE t (name TEXT)');
      const insert = db.prepare('INSERT INTO t VALUES (?)');
      for (let i = 0; i < 100; i++) insert.run(`name ${i}`);
      db.createCollation('c', compare);
      return db;
    }

    const error = new Error('boom');
    function compare() {
      throw error;
    }

    const sql = 'SELECT name FROM t ORDER BY name COLLATE c';
    const run = {
      all: (db) => db.prepare(sql).all(),
      get: (db) => db.prepare(sql).get(),
      iterate: (db) => db.prepare(sql).iterate().next(),
      run: (db) => db.prepare(sql).run(),
      exec: (db) => db.exec(sql),
      tagStore: (db) => db.createTagStore().all([sql]),
    };

    for (const [name, fn] of Object.entries(run)) {
      test(`are thrown by ${name}()`, (t) => {
        const db = setup(t, compare);
        assert.throws(() => fn(db), error);
        // The connection is still usable.
        db.createCollation('c', reverse);
        fn(db);
        assert.strictEqual(db.prepare(sql).get().name, 'name 99');
      });
    }

    test('stop compare from being called again', (t) => {
      let calls = 0;
      const db = setup(t, () => {
        calls++;
        throw error;
      });
      assert.throws(() => db.prepare(sql).all(), error);
      assert.strictEqual(calls, 1);
    });

    test('roll back the statement that failed', (t) => {
      const db = setup(t, compare);
      assert.throws(() => {
        db.exec('CREATE INDEX t_name ON t (name COLLATE c)');
      }, error);
      assert.deepStrictEqual(
        db.prepare("SELECT name FROM sqlite_schema WHERE type = 'index'").all(),
        []);
    });

    suite('roll back writes', () => {
      // The index is created while compare works, and inserting one row only
      // compares a few times, so SQLite finishes the statement before it
      // notices the interrupt.
      let fail;
      function indexed(t) {
        fail = false;
        const db = setup(t, (a, b) => {
          if (fail) throw error;
          return a < b ? -1 : a > b ? 1 : 0;
        });
        db.exec('CREATE INDEX t_name ON t (name COLLATE c)');
        fail = true;
        return db;
      }

      function check(db) {
        // The integrity check compares index entries too.
        fail = false;
        assert.strictEqual(db.isTransaction, false);
        assert.strictEqual(
          db.prepare('SELECT count(*) AS n FROM t').get().n, 100);
        assert.strictEqual(
          db.prepare('PRAGMA integrity_check').get().integrity_check, 'ok');
      }

      test('outside a transaction', (t) => {
        const db = indexed(t);
        assert.throws(() => {
          db.prepare("INSERT INTO t VALUES ('x')").run();
        }, error);
        check(db);
      });

      test('in exec()', (t) => {
        const db = indexed(t);
        assert.throws(() => {
          db.exec("INSERT INTO t VALUES ('x'); INSERT INTO t VALUES ('y');");
        }, error);
        check(db);
      });

      test('inside a transaction', (t) => {
        const db = indexed(t);
        db.exec('BEGIN');
        assert.throws(() => {
          db.prepare("INSERT INTO t VALUES ('x')").run();
        }, error);
        check(db);
      });

      test('committed by exec()', (t) => {
        const db = indexed(t);
        assert.throws(() => {
          db.exec("BEGIN; INSERT INTO t VALUES ('x'); COMMIT;");
        }, error);
        check(db);
      });
    });

    test('do not roll back a transaction for a read', (t) => {
      const db = setup(t, compare);
      db.exec("BEGIN; INSERT INTO t VALUES ('x');");
      assert.throws(() => db.prepare(sql).all(), error);
      assert.strictEqual(db.isTransaction, true);
      db.exec('COMMIT');
      assert.strictEqual(
        db.prepare('SELECT count(*) AS n FROM t').get().n, 101);
    });

    test('are not replaced by a function in the same statement', (t) => {
      const db = setup(t, compare);
      db.function('f', (value) => value);
      assert.throws(() => {
        db.prepare("SELECT f(name) FROM t WHERE name = 'x' COLLATE c").all();
      }, error);
    });

    test('include errors converting the result', (t) => {
      const db = setup(t, () => Symbol('nope'));
      assert.throws(() => db.prepare(sql).all(), {
        name: 'TypeError',
        message: /Cannot convert a Symbol value to a number/,
      });
    });
  });

  test('cannot close the database', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('b'), ('a');`);
    db.createCollation('c', () => db.close());
    assert.throws(() => {
      db.prepare('SELECT name FROM t ORDER BY name COLLATE c').all();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database cannot be closed while in a callback/,
    });
    assert.strictEqual(db.isOpen, true);
  });

  test('can run other queries', (t) => {
    const db = new Database(':memory:');
    t.after(() => db.close());
    db.exec(`CREATE TABLE t (name TEXT);
             INSERT INTO t VALUES ('b'), ('c'), ('a');
             CREATE TABLE rank (name TEXT, value INTEGER);
             INSERT INTO rank VALUES ('a', 3), ('b', 1), ('c', 2);`);
    const rank = db.prepare('SELECT value FROM rank WHERE name = ?');
    db.createCollation('ranked', (a, b) => {
      return rank.get(a).value - rank.get(b).value;
    });
    assert.deepStrictEqual(
      names(db, 'SELECT name FROM t ORDER BY name COLLATE ranked'),
      ['b', 'c', 'a']);
  });

  test('is released when the database is closed', async () => {
    const { gcUntil } = require('../common/gc');
    const ref = (() => {
      const db = new Database(':memory:');
      const compare = () => 0;
      db.createCollation('c', compare);
      db.close();
      return new WeakRef(compare);
    })();
    await gcUntil('collation is released', () => ref.deref() === undefined);
  });
});
