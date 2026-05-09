'use strict';
const { skipIfSQLiteMissing, mustCall } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

suite('DatabaseSync.prototype.function()', () => {
  suite('input validation', () => {
    const db = new DatabaseSync(':memory:');

    test('throws if name is not a string', () => {
      assert.throws(() => {
        db.function();
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "name" argument must be a string/,
      });
    });

    test('throws if function is not a function', () => {
      assert.throws(() => {
        db.function('foo');
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "function" argument must be a function/,
      });
    });

    test('throws if options is not an object', () => {
      assert.throws(() => {
        db.function('foo', null, () => {});
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options" argument must be an object/,
      });
    });

    test('throws if options.useBigIntArguments is not a boolean', () => {
      assert.throws(() => {
        db.function('foo', { useBigIntArguments: null }, () => {});
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.useBigIntArguments" argument must be a boolean/,
      });
    });

    test('throws if options.varargs is not a boolean', () => {
      assert.throws(() => {
        db.function('foo', { varargs: null }, () => {});
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.varargs" argument must be a boolean/,
      });
    });

    test('throws if options.deterministic is not a boolean', () => {
      assert.throws(() => {
        db.function('foo', { deterministic: null }, () => {});
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.deterministic" argument must be a boolean/,
      });
    });

    test('throws if options.directOnly is not a boolean', () => {
      assert.throws(() => {
        db.function('foo', { directOnly: null }, () => {});
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.directOnly" argument must be a boolean/,
      });
    });
  });

  suite('useBigIntArguments', () => {
    test('converts arguments to BigInts when true', () => {
      const db = new DatabaseSync(':memory:');
      let value;
      const r = db.function('custom', { useBigIntArguments: true }, (arg) => {
        value = arg;
      });
      assert.strictEqual(r, undefined);
      db.prepare('SELECT custom(5) AS custom').get();
      assert.strictEqual(value, 5n);
    });

    test('uses number primitives when false', () => {
      const db = new DatabaseSync(':memory:');
      let value;
      const r = db.function('custom', { useBigIntArguments: false }, (arg) => {
        value = arg;
      });
      assert.strictEqual(r, undefined);
      db.prepare('SELECT custom(5) AS custom').get();
      assert.strictEqual(value, 5);
    });

    test('defaults to false', () => {
      const db = new DatabaseSync(':memory:');
      let value;
      const r = db.function('custom', (arg) => {
        value = arg;
      });
      assert.strictEqual(r, undefined);
      db.prepare('SELECT custom(5) AS custom').get();
      assert.strictEqual(value, 5);
    });

    test('throws if value cannot fit in a number', () => {
      const db = new DatabaseSync(':memory:');
      const value = Number.MAX_SAFE_INTEGER + 1;
      db.function('custom', (arg) => {});
      assert.throws(() => {
        db.prepare(`SELECT custom(${value}) AS custom`).get();
      }, {
        code: 'ERR_OUT_OF_RANGE',
        message: /Value is too large to be represented as a JavaScript number: 9007199254740992/,
      });
    });
  });

  suite('varargs', () => {
    test('supports variable number of arguments when true', () => {
      const db = new DatabaseSync(':memory:');
      let value;
      const r = db.function('custom', { varargs: true }, (...args) => {
        value = args;
      });
      assert.strictEqual(r, undefined);
      db.prepare('SELECT custom(5, 4, 3, 2, 1) AS custom').get();
      assert.deepStrictEqual(value, [5, 4, 3, 2, 1]);
    });

    test('uses function.length when false', () => {
      const db = new DatabaseSync(':memory:');
      let value;
      const r = db.function('custom', { varargs: false }, (a, b, c) => {
        value = [a, b, c];
      });
      assert.strictEqual(r, undefined);
      db.prepare('SELECT custom(1, 2, 3) AS custom').get();
      assert.deepStrictEqual(value, [1, 2, 3]);
    });

    test('defaults to false', () => {
      const db = new DatabaseSync(':memory:');
      let value;
      const r = db.function('custom', (a, b, c) => {
        value = [a, b, c];
      });
      assert.strictEqual(r, undefined);
      db.prepare('SELECT custom(7, 8, 9) AS custom').get();
      assert.deepStrictEqual(value, [7, 8, 9]);
    });

    test('throws if an incorrect number of arguments is provided', () => {
      const db = new DatabaseSync(':memory:');
      db.function('custom', (a, b, c, d) => {});
      assert.throws(() => {
        db.prepare('SELECT custom(1, 2, 3) AS custom').get();
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /wrong number of arguments to function custom\(\)/,
      });
    });
  });

  suite('deterministic', () => {
    test('creates a deterministic function when true', () => {
      const db = new DatabaseSync(':memory:');
      db.function('isDeterministic', { deterministic: true }, () => {
        return 42;
      });
      const r = db.exec(`
        CREATE TABLE t1 (
          a INTEGER PRIMARY KEY,
          b INTEGER GENERATED ALWAYS AS (isDeterministic()) VIRTUAL
        )
      `);
      assert.strictEqual(r, undefined);
    });

    test('creates a non-deterministic function when false', () => {
      const db = new DatabaseSync(':memory:');
      db.function('isNonDeterministic', { deterministic: false }, () => {
        return 42;
      });
      assert.throws(() => {
        db.exec(`
          CREATE TABLE t1 (
            a INTEGER PRIMARY KEY,
            b INTEGER GENERATED ALWAYS AS (isNonDeterministic()) VIRTUAL
          )
        `);
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /non-deterministic functions prohibited in generated columns/,
      });
    });

    test('deterministic defaults to false', () => {
      const db = new DatabaseSync(':memory:');
      db.function('isNonDeterministic', () => {
        return 42;
      });
      assert.throws(() => {
        db.exec(`
          CREATE TABLE t1 (
            a INTEGER PRIMARY KEY,
            b INTEGER GENERATED ALWAYS AS (isNonDeterministic()) VIRTUAL
          )
        `);
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /non-deterministic functions prohibited in generated columns/,
      });
    });
  });

  suite('directOnly', () => {
    test('sets SQLite direct only flag when true', () => {
      const db = new DatabaseSync(':memory:');
      db.function('fn', { deterministic: true, directOnly: true }, () => {
        return 42;
      });
      assert.throws(() => {
        db.exec(`
          CREATE TABLE t1 (
            a INTEGER PRIMARY KEY,
            b INTEGER GENERATED ALWAYS AS (fn()) VIRTUAL
          )
        `);
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /unsafe use of fn\(\)/
      });
    });

    test('does not set SQLite direct only flag when false', () => {
      const db = new DatabaseSync(':memory:');
      db.function('fn', { deterministic: true, directOnly: false }, () => {
        return 42;
      });
      const r = db.exec(`
        CREATE TABLE t1 (
          a INTEGER PRIMARY KEY,
          b INTEGER GENERATED ALWAYS AS (fn()) VIRTUAL
        )
      `);
      assert.strictEqual(r, undefined);
    });

    test('directOnly defaults to false', () => {
      const db = new DatabaseSync(':memory:');
      db.function('fn', { deterministic: true }, () => {
        return 42;
      });
      const r = db.exec(`
        CREATE TABLE t1 (
          a INTEGER PRIMARY KEY,
          b INTEGER GENERATED ALWAYS AS (fn()) VIRTUAL
        )
      `);
      assert.strictEqual(r, undefined);
    });
  });

  suite('return types', () => {
    test('supported return types', () => {
      const db = new DatabaseSync(':memory:');
      db.function('retUndefined', () => {});
      db.function('retNull', () => { return null; });
      db.function('retNumber', () => { return 3; });
      db.function('retString', () => { return 'foo'; });
      db.function('retBigInt', () => { return 5n; });
      db.function('retUint8Array', () => { return new Uint8Array([1, 2, 3]); });
      db.function('retArrayBufferView', () => {
        const arrayBuffer = new Uint8Array([1, 2, 3]).buffer;
        return new DataView(arrayBuffer);
      });
      const stmt = db.prepare(`SELECT
        retUndefined() AS retUndefined,
        retNull() AS retNull,
        retNumber() AS retNumber,
        retString() AS retString,
        retBigInt() AS retBigInt,
        retUint8Array() AS retUint8Array,
        retArrayBufferView() AS retArrayBufferView
      `);
      assert.deepStrictEqual(stmt.get(), {
        __proto__: null,
        retUndefined: null,
        retNull: null,
        retNumber: 3,
        retString: 'foo',
        retBigInt: 5,
        retUint8Array: new Uint8Array([1, 2, 3]),
        retArrayBufferView: new Uint8Array([1, 2, 3]),
      });
    });

    test('throws if returned BigInt is too large for SQLite', () => {
      const db = new DatabaseSync(':memory:');
      db.function('retBigInt', () => {
        return BigInt(Number.MAX_SAFE_INTEGER + 1);
      });
      const stmt = db.prepare('SELECT retBigInt() AS retBigInt');
      assert.throws(() => {
        stmt.get();
      }, {
        code: 'ERR_OUT_OF_RANGE',
      });
    });

    test('does not support Promise return values', () => {
      const db = new DatabaseSync(':memory:');
      db.function('retPromise', async () => {});
      const stmt = db.prepare('SELECT retPromise() AS retPromise');
      assert.throws(() => {
        stmt.get();
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /Asynchronous user-defined functions are not supported/,
      });
    });

    test('throws on unsupported return types', () => {
      const db = new DatabaseSync(':memory:');
      db.function('retFunction', () => {
        return () => {};
      });
      const stmt = db.prepare('SELECT retFunction() AS retFunction');
      assert.throws(() => {
        stmt.get();
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /Returned JavaScript value cannot be converted to a SQLite value/,
      });
    });
  });

  suite('handles conflicting errors from SQLite and JavaScript', () => {
    test('throws if value cannot fit in a number', () => {
      const db = new DatabaseSync(':memory:');
      const expected = { __proto__: null, id: 5, data: 'foo' };
      db.function('custom', (arg) => {});
      db.exec('CREATE TABLE test (id NUMBER NOT NULL PRIMARY KEY, data TEXT)');
      db.prepare('INSERT INTO test (id, data) VALUES (?, ?)').run(5, 'foo');
      assert.deepStrictEqual(db.prepare('SELECT * FROM test').get(), expected);
      assert.throws(() => {
        db.exec(`UPDATE test SET data = CUSTOM(${Number.MAX_SAFE_INTEGER + 1})`);
      }, {
        code: 'ERR_OUT_OF_RANGE',
        message: /Value is too large to be represented as a JavaScript number: 9007199254740992/,
      });
      assert.deepStrictEqual(db.prepare('SELECT * FROM test').get(), expected);
    });

    test('propagates JavaScript errors', () => {
      const db = new DatabaseSync(':memory:');
      const expected = { __proto__: null, id: 5, data: 'foo' };
      const err = new Error('boom');
      db.function('throws', () => {
        throw err;
      });
      db.exec('CREATE TABLE test (id NUMBER NOT NULL PRIMARY KEY, data TEXT)');
      db.prepare('INSERT INTO test (id, data) VALUES (?, ?)').run(5, 'foo');
      assert.deepStrictEqual(db.prepare('SELECT * FROM test').get(), expected);
      assert.throws(() => {
        db.exec('UPDATE test SET data = THROWS()');
      }, err);
      assert.deepStrictEqual(db.prepare('SELECT * FROM test').get(), expected);
    });
  });

  test('supported argument types', () => {
    const db = new DatabaseSync(':memory:');
    db.function('arguments', mustCall((i, f, s, n, b) => {
      assert.strictEqual(i, 5);
      assert.strictEqual(f, 3.14);
      assert.strictEqual(s, 'foo');
      assert.strictEqual(n, null);
      assert.deepStrictEqual(b, new Uint8Array([254]));
      return 42;
    }));
    const stmt = db.prepare(
      'SELECT arguments(5, 3.14, \'foo\', null, x\'fe\') as result'
    );
    assert.deepStrictEqual(stmt.get(), { __proto__: null, result: 42 });
  });

  test('propagates thrown errors', () => {
    const db = new DatabaseSync(':memory:');
    const err = new Error('boom');
    db.function('throws', () => {
      throw err;
    });
    const stmt = db.prepare('SELECT throws()');
    assert.throws(() => {
      stmt.get();
    }, err);
  });

  test('throws if database is not open', () => {
    const db = new DatabaseSync(':memory:', { open: false });
    assert.throws(() => {
      db.function('foo', () => {});
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  suite('reentrant database operations from inside a user function callback', () => {
    const reentrantCloseError = {
      code: 'ERR_INVALID_STATE',
      message: /database cannot be closed inside a user-defined function callback/,
    };
    const reentrantStmtError = {
      code: 'ERR_INVALID_STATE',
      message: /statement is currently being executed/,
    };

    function newDbWithRows() {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.prepare('INSERT INTO t VALUES (1, 10)').run();
      db.prepare('INSERT INTO t VALUES (2, 20)').run();
      return db;
    }

    test('.all() rejects db.close() and completes without crashing', () => {
      const db = newDbWithRows();
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }, 2));
      const rows = db.prepare('SELECT close_db(v) AS v FROM t').all();
      assert.deepStrictEqual(rows, [
        { __proto__: null, v: 10 },
        { __proto__: null, v: 20 },
      ]);
      assert.strictEqual(db.isOpen, true);
    });

    test('.get() rejects db.close() and completes without crashing', () => {
      const db = newDbWithRows();
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }));
      const row = db.prepare('SELECT close_db(v) AS v FROM t').get();
      assert.deepStrictEqual(row, { __proto__: null, v: 10 });
      assert.strictEqual(db.isOpen, true);
    });

    test('.run() rejects db.close() and completes without crashing', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }, 2));
      assert.deepStrictEqual(
        db.prepare('INSERT INTO t SELECT close_db(1), close_db(2)').run(),
        { changes: 1, lastInsertRowid: 1 },
      );
      assert.strictEqual(db.isOpen, true);
    });

    test('iterator rejects db.close() and completes without crashing', () => {
      const db = newDbWithRows();
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }, 2));
      const iter = db.prepare('SELECT close_db(v) AS v FROM t').iterate();
      assert.deepStrictEqual(iter.next(), {
        __proto__: null,
        done: false,
        value: { __proto__: null, v: 10 },
      });
      assert.deepStrictEqual(iter.next(), {
        __proto__: null,
        done: false,
        value: { __proto__: null, v: 20 },
      });
      assert.deepStrictEqual(iter.next(), {
        __proto__: null,
        done: true,
        value: null,
      });
      assert.strictEqual(db.isOpen, true);
    });

    test('SQL tag store .run() rejects db.close() and completes without crashing', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }, 2));
      const sql = db.createTagStore(4);
      assert.deepStrictEqual(
        sql.run`INSERT INTO t SELECT close_db(${1}), close_db(${2})`,
        { changes: 1, lastInsertRowid: 1 },
      );
      assert.strictEqual(db.isOpen, true);
    });

    test('SQL tag store .all() rejects db.close() and completes without crashing', () => {
      const db = newDbWithRows();
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }, 2));
      const sql = db.createTagStore(4);
      const rows = sql.all`SELECT close_db(v) AS v FROM t`;
      assert.deepStrictEqual(rows, [
        { __proto__: null, v: 10 },
        { __proto__: null, v: 20 },
      ]);
      assert.strictEqual(db.isOpen, true);
    });

    test('SQL tag store .get() rejects db.close() and completes without crashing', () => {
      const db = newDbWithRows();
      db.function('close_db', mustCall((x) => {
        assert.throws(() => db.close(), reentrantCloseError);
        return x;
      }));
      const sql = db.createTagStore(4);
      const row = sql.get`SELECT close_db(v) AS v FROM t`;
      assert.deepStrictEqual(row, { __proto__: null, v: 10 });
      assert.strictEqual(db.isOpen, true);
    });

    test('uncaught db.close() failure is propagated from the callback', () => {
      const db = new DatabaseSync(':memory:');
      db.function('close_db', () => db.close());
      assert.throws(
        () => db.prepare('SELECT close_db()').get(),
        reentrantCloseError,
      );
      assert.strictEqual(db.isOpen, true);
    });

    test('resetting a statement from a callback is rejected', () => {
      const db = new DatabaseSync(':memory:');
      let stmt;
      db.function('x', () => stmt.get());
      stmt = db.prepare('SELECT x()');
      assert.throws(() => stmt.get(), reentrantStmtError);
      assert.strictEqual(db.isOpen, true);
    });

    test('reentrant iter.next() from a callback is rejected', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.prepare('INSERT INTO t VALUES (1, 10)').run();
      let iter;
      db.function('reenter', mustCall(() => {
        assert.throws(() => iter.next(), reentrantStmtError);
        return 0;
      }));
      iter = db.prepare('SELECT reenter() FROM t').iterate();
      assert.strictEqual(iter.next().done, false);
      assert.strictEqual(iter.next().done, true);
      assert.strictEqual(db.isOpen, true);
    });

    test('aggregate step rejects db.close() and completes', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.prepare('INSERT INTO t VALUES (1, 10)').run();
      db.prepare('INSERT INTO t VALUES (2, 20)').run();
      db.aggregate('agg_close', {
        start: 0,
        step: mustCall((acc, v) => {
          assert.throws(() => db.close(), reentrantCloseError);
          return acc + v;
        }, 2),
      });
      assert.deepStrictEqual(
        db.prepare('SELECT agg_close(v) AS s FROM t').get(),
        { __proto__: null, s: 30 },
      );
      assert.strictEqual(db.isOpen, true);
    });

    test('aggregate result rejects db.close() and completes', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.prepare('INSERT INTO t VALUES (1, 10)').run();
      db.aggregate('agg_result', {
        start: 0,
        step: (acc, v) => acc + v,
        result: mustCall((acc) => {
          assert.throws(() => db.close(), reentrantCloseError);
          return acc;
        }),
      });
      assert.deepStrictEqual(
        db.prepare('SELECT agg_result(v) AS s FROM t').get(),
        { __proto__: null, s: 10 },
      );
      assert.strictEqual(db.isOpen, true);
    });

    test('aggregate start function rejects db.close() and completes', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.prepare('INSERT INTO t VALUES (1, 10)').run();
      db.aggregate('agg_start', {
        start: mustCall(() => {
          assert.throws(() => db.close(), reentrantCloseError);
          return 0;
        }),
        step: (acc, v) => acc + v,
      });
      assert.deepStrictEqual(
        db.prepare('SELECT agg_start(v) AS s FROM t').get(),
        { __proto__: null, s: 10 },
      );
      assert.strictEqual(db.isOpen, true);
    });

    test('SQL tag store clear from a callback is rejected', () => {
      const db = new DatabaseSync(':memory:');
      const sql = db.createTagStore(4);
      assert.deepStrictEqual(sql.get`SELECT 1 AS one`, { __proto__: null, one: 1 });
      db.function('x', () => sql.clear());
      assert.throws(() => db.prepare('SELECT x()').get(), {
        code: 'ERR_INVALID_STATE',
        message: /tag store cannot be cleared inside a user-defined function callback/,
      });
      assert.strictEqual(sql.size, 1);
    });

    // Regression: a user function may run other prepared statements on
    // the same connection (the "lookup" pattern). Only the *currently
    // running* statement is unsafe to step/reset/finalize.
    test('callback can run a different prepared statement on the same db', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE lookup (id INTEGER PRIMARY KEY, v INTEGER)');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY)');
      db.prepare('INSERT INTO lookup VALUES (1, 100), (2, 200)').run();
      db.prepare('INSERT INTO t VALUES (1), (2)').run();
      const lookup = db.prepare('SELECT v FROM lookup WHERE id = ?');
      db.function('lookup_v', mustCall((id) => lookup.get(id).v, 2));
      assert.deepStrictEqual(
        db.prepare('SELECT lookup_v(id) AS v FROM t ORDER BY id').all(),
        [
          { __proto__: null, v: 100 },
          { __proto__: null, v: 200 },
        ],
      );
    });

    test('callback can use SQL tag store with different SQL', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER)');
      db.prepare('INSERT INTO t VALUES (1, 10), (2, 20)').run();
      const sql = db.createTagStore(4);
      db.function('double_v', mustCall((id) => {
        return sql.get`SELECT v * 2 AS d FROM t WHERE id = ${id}`.d;
      }, 2));
      assert.deepStrictEqual(
        db.prepare('SELECT double_v(id) AS d FROM t ORDER BY id').all(),
        [
          { __proto__: null, d: 20 },
          { __proto__: null, d: 40 },
        ],
      );
    });

    // Cover IsStepping rejection on every JS-callable statement method.
    for (const method of ['all', 'run', 'iterate']) {
      test(`stmt.${method}() same-stmt reentry is rejected`, () => {
        const db = new DatabaseSync(':memory:');
        let stmt;
        db.function('x', mustCall(() => {
          assert.throws(() => stmt[method](), reentrantStmtError);
          return 0;
        }));
        stmt = db.prepare('SELECT x()');
        if (method === 'iterate') {
          stmt.iterate().next();
        } else {
          stmt[method]();
        }
      });
    }

    test('iter.return() same-stmt reentry is rejected', () => {
      const db = new DatabaseSync(':memory:');
      let iter;
      db.function('x', mustCall(() => {
        assert.throws(() => iter.return(), reentrantStmtError);
        return 0;
      }));
      iter = db.prepare('SELECT x()').iterate();
      iter.next();
    });

    // Cover IsStepping rejection on every SQLTagStore execution method.
    for (const method of ['run', 'get', 'all', 'iterate']) {
      test(`sql.${method}\`...\` same-stmt reentry is rejected`, () => {
        const db = new DatabaseSync(':memory:');
        const sql = db.createTagStore(4);
        db.function('x', mustCall(() => {
          // Re-running the same tag returns the cached stmt — which is
          // currently stepping — so the entry check throws.
          assert.throws(() => sql[method]`SELECT x()`, reentrantStmtError);
          return 0;
        }));
        if (method === 'iterate') {
          sql.iterate`SELECT x()`.next();
        } else {
          assert.ok(sql[method]`SELECT x()`);
        }
      });
    }

    test('db.deserialize() from a callback is rejected', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t (id INTEGER)');
      const snapshot = db.serialize();
      db.function('x', mustCall(() => {
        assert.throws(() => db.deserialize(snapshot), {
          code: 'ERR_INVALID_STATE',
          message: /database operation is not allowed inside a user-defined function callback/,
        });
        return 0;
      }));
      db.prepare('SELECT x()').get();
      assert.strictEqual(db.isOpen, true);
    });

    test('db[Symbol.dispose]() is a no-op when invoked from a callback', () => {
      const db = new DatabaseSync(':memory:');
      db.function('do_dispose', mustCall(() => {
        db[Symbol.dispose]();
        return 1;
      }));
      // The dispose attempt is silently skipped; the outer query still
      // completes and the database stays open.
      assert.deepStrictEqual(
        db.prepare('SELECT do_dispose() AS x').get(),
        { __proto__: null, x: 1 },
      );
      assert.strictEqual(db.isOpen, true);
      // Outside of any callback, dispose works normally.
      db[Symbol.dispose]();
      assert.strictEqual(db.isOpen, false);
    });
  });
});
