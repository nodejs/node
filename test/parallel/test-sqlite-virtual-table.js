// Flags: --expose-gc
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

suite('DatabaseSync.prototype.createModule()', () => {
  suite('input validation', () => {
    const db = new DatabaseSync(':memory:');

    test('throws if name is not a string', () => {
      assert.throws(() => {
        db.createModule();
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "name" argument must be a string/,
      });
    });

    test('throws if options is not an object', () => {
      assert.throws(() => {
        db.createModule('mod', null);
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options" argument must be an object/,
      });
    });

    test('throws if options.columns is not an array', () => {
      assert.throws(() => {
        db.createModule('mod', { columns: 'bad', rows() {} });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.columns" argument must be an array/,
      });
    });

    test('throws if options.columns is empty', () => {
      assert.throws(() => {
        db.createModule('mod', { columns: [], rows() {} });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /The "options\.columns" array must not be empty/,
      });
    });

    test('throws if options.rows is not a function', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 'x', type: 'TEXT' }],
          rows: 'bad',
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.rows" argument must be a function/,
      });
    });

    test('throws if column name is not a string', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 123, type: 'TEXT' }],
          rows() {},
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The column "name" property must be a string/,
      });
    });

    test('throws if column type is not a string', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 'x', type: 123 }],
          rows() {},
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The column "type" property must be a string/,
      });
    });

    test('throws if column type is not a valid SQLite type', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 'x', type: 'INVALID' }],
          rows() {},
        });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /The column "type" property must be one of/,
      });
    });

    test('throws if column hidden is not a boolean', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 'x', type: 'TEXT', hidden: 'yes' }],
          rows() {},
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The column "hidden" property must be a boolean/,
      });
    });

    test('throws if options.directOnly is not a boolean', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 'x', type: 'TEXT' }],
          rows() {},
          directOnly: 'yes',
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.directOnly" argument must be a boolean/,
      });
    });

    test('throws if options.useBigIntArguments is not a boolean', () => {
      assert.throws(() => {
        db.createModule('mod', {
          columns: [{ name: 'x', type: 'TEXT' }],
          rows() {},
          useBigIntArguments: 'yes',
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.useBigIntArguments" argument must be a boolean/,
      });
    });

    test('throws if database is not open', () => {
      const closedDb = new DatabaseSync(':memory:');
      closedDb.close();
      assert.throws(() => {
        closedDb.createModule('mod', {
          columns: [{ name: 'x', type: 'TEXT' }],
          rows() {},
        });
      }, {
        code: 'ERR_INVALID_STATE',
        message: /database is not open/,
      });
    });
  });

  suite('basic virtual table', () => {
    test('creates a simple read-only virtual table', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('simple', {
        columns: [
          { name: 'id', type: 'INTEGER' },
          { name: 'name', type: 'TEXT' },
        ],
        *rows() {
          yield [1, 'Alice'];
          yield [2, 'Bob'];
          yield [3, 'Charlie'];
        },
      });

      db.exec('CREATE VIRTUAL TABLE t1 USING simple');
      const result = db.prepare('SELECT * FROM t1').all();
      assert.deepStrictEqual(result, [
        { __proto__: null, id: 1, name: 'Alice' },
        { __proto__: null, id: 2, name: 'Bob' },
        { __proto__: null, id: 3, name: 'Charlie' },
      ]);
    });

    test('works as eponymous table (without CREATE VIRTUAL TABLE)', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('eponymous', {
        columns: [
          { name: 'value', type: 'TEXT' },
        ],
        *rows() {
          yield ['hello'];
          yield ['world'];
        },
      });

      const result = db.prepare('SELECT * FROM eponymous').all();
      assert.deepStrictEqual(result, [
        { __proto__: null, value: 'hello' },
        { __proto__: null, value: 'world' },
      ]);
    });

    test('supports rows() returning an array', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('array_mod', {
        columns: [
          { name: 'val', type: 'INTEGER' },
        ],
        rows() {
          return [[10], [20], [30]];
        },
      });

      const result = db.prepare('SELECT * FROM array_mod').all();
      assert.deepStrictEqual(result, [
        { __proto__: null, val: 10 },
        { __proto__: null, val: 20 },
        { __proto__: null, val: 30 },
      ]);
    });

    test('supports empty result set', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('empty_mod', {
        columns: [
          { name: 'x', type: 'TEXT' },
        ],
        *rows() {
          // yields nothing
        },
      });

      const result = db.prepare('SELECT * FROM empty_mod').all();
      assert.deepStrictEqual(result, []);
    });
  });

  suite('table-valued function with parameters', () => {
    test('passes hidden column values as arguments to rows()', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('gen_series', {
        columns: [
          { name: 'value', type: 'INTEGER' },
          { name: 'start', type: 'INTEGER', hidden: true },
          { name: 'stop', type: 'INTEGER', hidden: true },
          { name: 'step', type: 'INTEGER', hidden: true },
        ],
        *rows(start, stop, step) {
          start ??= 0;
          stop ??= 10;
          step ??= 1;
          for (let i = start; i <= stop; i += step) {
            yield [i];
          }
        },
      });

      const result = db.prepare(
        'SELECT value FROM gen_series(1, 5, 1)'
      ).all();
      assert.deepStrictEqual(result, [
        { __proto__: null, value: 1 },
        { __proto__: null, value: 2 },
        { __proto__: null, value: 3 },
        { __proto__: null, value: 4 },
        { __proto__: null, value: 5 },
      ]);
    });

    test('passes step parameter', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('gen_step', {
        columns: [
          { name: 'value', type: 'INTEGER' },
          { name: 'start', type: 'INTEGER', hidden: true },
          { name: 'stop', type: 'INTEGER', hidden: true },
          { name: 'step', type: 'INTEGER', hidden: true },
        ],
        *rows(start, stop, step) {
          start ??= 0;
          stop ??= 10;
          step ??= 1;
          for (let i = start; i <= stop; i += step) {
            yield [i];
          }
        },
      });

      const result = db.prepare(
        'SELECT value FROM gen_step(0, 10, 3)'
      ).all();
      assert.deepStrictEqual(result, [
        { __proto__: null, value: 0 },
        { __proto__: null, value: 3 },
        { __proto__: null, value: 6 },
        { __proto__: null, value: 9 },
      ]);
    });

    test('handles partial parameters (some hidden cols unconstrained)', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('partial_params', {
        columns: [
          { name: 'value', type: 'INTEGER' },
          { name: 'count', type: 'INTEGER', hidden: true },
        ],
        *rows(count) {
          const n = count ?? 3;
          for (let i = 0; i < n; i++) {
            yield [i];
          }
        },
      });

      // Without parameter (uses default).
      db.exec('CREATE VIRTUAL TABLE pp USING partial_params');
      const result1 = db.prepare('SELECT * FROM pp').all();
      assert.strictEqual(result1.length, 3);

      // With parameter via table-valued function syntax.
      const result2 = db.prepare('SELECT * FROM partial_params(5)').all();
      assert.strictEqual(result2.length, 5);
    });

    test('maps parameters correctly beyond 32 hidden columns', () => {
      const db = new DatabaseSync(':memory:');
      const paramCount = 40;
      const columns = [{ name: 'value', type: 'INTEGER' }];
      for (let i = 0; i < paramCount; i++) {
        columns.push({ name: `p${i}`, type: 'INTEGER', hidden: true });
      }

      let received;
      db.createModule('many_params', {
        columns,
        rows(...args) {
          received = args;
          return [[0]];
        },
      });

      // Constrain only the last parameter. A bitmask in an int would not be
      // able to represent an index this high.
      db.prepare(`SELECT value FROM many_params(${
        Array.from({ length: paramCount }, (_, i) => (i === paramCount - 1 ? '7' : 'NULL')).join(', ')
      })`).all();

      assert.strictEqual(received.length, paramCount);
      assert.strictEqual(received[paramCount - 1], 7);
    });
  });

  suite('type conversions', () => {
    test('handles various SQLite data types', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('types_mod', {
        columns: [
          { name: 'int_col', type: 'INTEGER' },
          { name: 'real_col', type: 'REAL' },
          { name: 'text_col', type: 'TEXT' },
          { name: 'blob_col', type: 'BLOB' },
          { name: 'null_col', type: 'TEXT' },
        ],
        *rows() {
          yield [42, 3.14, 'hello', new Uint8Array([1, 2, 3]), null];
        },
      });

      const result = db.prepare('SELECT * FROM types_mod').get();
      assert.strictEqual(result.int_col, 42);
      assert.strictEqual(result.real_col, 3.14);
      assert.strictEqual(result.text_col, 'hello');
      assert.deepStrictEqual(
        new Uint8Array(result.blob_col),
        new Uint8Array([1, 2, 3])
      );
      assert.strictEqual(result.null_col, null);
    });
  });

  suite('useBigIntArguments', () => {
    test('passes INTEGER parameters as BigInts when enabled', () => {
      const db = new DatabaseSync(':memory:');
      let receivedType;

      db.createModule('bigint_mod', {
        columns: [
          { name: 'result', type: 'TEXT' },
          { name: 'input', type: 'INTEGER', hidden: true },
        ],
        useBigIntArguments: true,
        *rows(input) {
          receivedType = typeof input;
          yield [String(input)];
        },
      });

      db.prepare('SELECT * FROM bigint_mod(42)').get();
      assert.strictEqual(receivedType, 'bigint');
    });

    test('passes INTEGER parameters as numbers by default', () => {
      const db = new DatabaseSync(':memory:');
      let receivedType;

      db.createModule('number_mod', {
        columns: [
          { name: 'result', type: 'TEXT' },
          { name: 'input', type: 'INTEGER', hidden: true },
        ],
        *rows(input) {
          receivedType = typeof input;
          yield [String(input)];
        },
      });

      db.prepare('SELECT * FROM number_mod(42)').get();
      assert.strictEqual(receivedType, 'number');
    });
  });

  suite('re-entrancy', () => {
    // Closing the database while SQLite is stepping the statement that owns
    // the cursor would finalize that statement from under sqlite3_step().
    for (const [name, makeRows] of [
      ['rows()', (db) => function* () { db.close(); yield [1]; }],
      ['iterator next()', (db) => () => ({
        [Symbol.iterator]() { return this; },
        next() { db.close(); return { value: [1], done: false }; },
      })],
      ['a row getter', (db) => () => [{ get 0() { db.close(); return 1; } }]],
    ]) {
      test(`throws when close() is called from ${name}`, () => {
        const db = new DatabaseSync(':memory:');
        db.createModule('closer', {
          columns: [{ name: 'value', type: 'INTEGER' }],
          rows: makeRows(db),
        });

        assert.throws(() => {
          db.prepare('SELECT value FROM closer').all();
        }, {
          code: 'ERR_INVALID_STATE',
          message: /database cannot be closed while in a callback/,
        });
      });
    }

    test('throws when createModule() is called from an authorizer', () => {
      const db = new DatabaseSync(':memory:');
      db.exec('CREATE TABLE t(a)');
      let err;
      db.setAuthorizer(() => {
        try {
          db.createModule('from_authz', {
            columns: [{ name: 'v', type: 'INTEGER' }],
            *rows() { yield [1]; },
          });
        } catch (e) {
          err = e;
        }
        return 0;
      });

      db.prepare('SELECT * FROM t').all();
      assert.strictEqual(err?.code, 'ERR_INVALID_STATE');
      assert.match(err.message, /cannot be accessed from an authorizer/);
    });
  });

  suite('iterator cleanup', () => {
    test('closes the iterator when SQLite stops stepping early', () => {
      const db = new DatabaseSync(':memory:');
      let cleanedUp = false;

      db.createModule('early_stop', {
        columns: [{ name: 'value', type: 'INTEGER' }],
        *rows() {
          try {
            for (let i = 0; i < 100; i++) {
              yield [i];
            }
          } finally {
            cleanedUp = true;
          }
        },
      });

      const rows = db.prepare('SELECT value FROM early_stop LIMIT 2').all();
      assert.strictEqual(rows.length, 2);
      assert.strictEqual(cleanedUp, true);
    });

    test('keeps the original error when rows() throws', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('cleanup_throws', {
        columns: [{ name: 'value', type: 'INTEGER' }],
        *rows() {
          try {
            yield [1];
            throw new Error('boom');
          } finally {
            // Cleanup must not mask the error above.
          }
        },
      });

      assert.throws(() => {
        db.prepare('SELECT value FROM cleanup_throws').all();
      }, /boom/);
    });

    test('closes the iterator on break out of a for...of loop', () => {
      const db = new DatabaseSync(':memory:');
      let cleanedUp = false;

      db.createModule('breaker', {
        columns: [{ name: 'v', type: 'INTEGER' }],
        *rows() {
          try {
            for (let i = 0; i < 1000; i++) {
              yield [i];
            }
          } finally {
            cleanedUp = true;
          }
        },
      });

      for (const row of db.prepare('SELECT v FROM breaker').iterate()) {
        if (row.v === 1) break;
      }
      assert.strictEqual(cleanedUp, true);
    });

    test('surfaces an error thrown by the iterator\'s return()', () => {
      const db = new DatabaseSync(':memory:');

      for (const [label, descriptor] of [
        ['method', { value() { throw new Error('return boom'); } }],
        ['getter', { get() { throw new Error('return boom'); } }],
      ]) {
        db.createModule(`ret_${label}`, {
          columns: [{ name: 'v', type: 'INTEGER' }],
          rows() {
            let i = 0;
            const it = {
              [Symbol.iterator]() { return this; },
              next() { return { value: [i++], done: i > 50 }; },
            };
            Object.defineProperty(it, 'return', descriptor);
            return it;
          },
        });

        assert.throws(() => {
          db.prepare(`SELECT v FROM ret_${label} LIMIT 2`).all();
        }, /return boom/, `throwing return ${label} should surface`);
      }
    });

    test('does not run cleanup when the statement is collected', () => {
      // The destructor runs from a GC callback, where JavaScript cannot be
      // executed. An abandoned generator does not run `finally` in JavaScript
      // either, so the expected outcome is no crash and no cleanup.
      const db = new DatabaseSync(':memory:');
      let cleanedUp = false;

      db.createModule('abandoned', {
        columns: [{ name: 'v', type: 'INTEGER' }],
        *rows() {
          try {
            for (let i = 0; i < 1000; i++) {
              yield [i];
            }
          } finally {
            cleanedUp = true;
          }
        },
      });

      (function abandon() {
        db.prepare('SELECT v FROM abandoned').iterate().next();
      })();

      for (let i = 0; i < 5; i++) {
        globalThis.gc({ execution: 'sync' });
      }
      assert.strictEqual(cleanedUp, false);
    });
  });

  suite('iteration protocol violations', () => {
    // Each of these is a misuse with no pending JavaScript exception, so the
    // module has to report a SQLite error of its own rather than relying on one.
    for (const [name, rows, expected] of [
      [
        'rows() returns a non-object',
        () => 42,
        /must return an iterable object/,
      ],
      [
        'Symbol.iterator returns a non-object',
        () => ({ [Symbol.iterator]() { return 7; } }),
        /Symbol\.iterator method must return an object/,
      ],
      [
        'iterator has no next()',
        () => ({ [Symbol.iterator]() { return this; } }),
        /must have a next\(\) method/,
      ],
      [
        'next() returns a non-object',
        () => ({ [Symbol.iterator]() { return this; }, next() { return 42; } }),
        /next\(\) method must return an object/,
      ],
    ]) {
      test(`reports an error when ${name}`, () => {
        const db = new DatabaseSync(':memory:');
        db.createModule('bad', {
          columns: [{ name: 'v', type: 'INTEGER' }],
          rows,
        });

        // Both entry points must fail; exec() has no return value to inspect,
        // so a missing error there would look like success.
        assert.throws(() => {
          db.prepare('SELECT v FROM bad').all();
        }, { code: 'ERR_SQLITE_ERROR', message: expected });

        assert.throws(() => {
          db.exec('SELECT v FROM bad');
        }, { code: 'ERR_SQLITE_ERROR', message: expected });
      });
    }
  });

  suite('hidden column constraints', () => {
    const makeDb = () => {
      const db = new DatabaseSync(':memory:');
      db.createModule('gs', {
        columns: [
          { name: 'value', type: 'INTEGER' },
          { name: 'start', type: 'INTEGER', hidden: true },
          { name: 'stop', type: 'INTEGER', hidden: true },
        ],
        *rows(start, stop) {
          start ??= 1;
          stop ??= 3;
          for (let i = start; i <= stop; i++) {
            yield [i];
          }
        },
      });
      return db;
    };

    test('reports the constrained value back from a hidden column', () => {
      const db = makeDb();
      assert.deepStrictEqual(
        db.prepare('SELECT start, value FROM gs(1, 3)').all(),
        [
          { __proto__: null, start: 1, value: 1 },
          { __proto__: null, start: 1, value: 2 },
          { __proto__: null, start: 1, value: 3 },
        ]);
    });

    test('survives a recheck of a constraint it already consumed', () => {
      // SQLite treats `omit` as a hint, so it may recheck `start = 1` against
      // whatever xColumn reports. Returning NULL there rejects every row.
      const db = makeDb();
      assert.deepStrictEqual(
        db.prepare('SELECT value FROM gs(1, 3) WHERE start = 1').all(),
        [
          { __proto__: null, value: 1 },
          { __proto__: null, value: 2 },
          { __proto__: null, value: 3 },
        ]);
    });

    test('uses the constrained plan when parameters come from a join', () => {
      const db = makeDb();
      db.exec('CREATE TABLE t(a); INSERT INTO t VALUES (1), (2)');
      assert.deepStrictEqual(
        db.prepare(
          'SELECT t.a, gs.value FROM t, gs(t.a, t.a + 1) AS gs ORDER BY t.a, gs.value'
        ).all(),
        [
          { __proto__: null, a: 1, value: 1 },
          { __proto__: null, a: 1, value: 2 },
          { __proto__: null, a: 2, value: 2 },
          { __proto__: null, a: 2, value: 3 },
        ]);
    });
  });

  suite('error handling', () => {
    test('propagates errors thrown in rows()', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('error_mod', {
        columns: [
          { name: 'x', type: 'TEXT' },
        ],
        rows() {
          throw new Error('rows error');
        },
      });

      assert.throws(() => {
        db.prepare('SELECT * FROM error_mod').all();
      }, {
        message: /rows error/,
      });
    });

    test('propagates errors thrown during iteration', () => {
      const db = new DatabaseSync(':memory:');

      db.createModule('iter_error_mod', {
        columns: [
          { name: 'x', type: 'INTEGER' },
        ],
        *rows() {
          yield [1];
          throw new Error('iteration error');
        },
      });

      assert.throws(() => {
        db.prepare('SELECT * FROM iter_error_mod').all();
      }, {
        message: /iteration error/,
      });
    });
  });

  suite('multiple queries', () => {
    test('supports querying the virtual table multiple times', () => {
      const db = new DatabaseSync(':memory:');
      let callCount = 0;

      db.createModule('multi_mod', {
        columns: [
          { name: 'value', type: 'INTEGER' },
        ],
        *rows() {
          callCount++;
          yield [callCount];
        },
      });

      db.exec('CREATE VIRTUAL TABLE m USING multi_mod');
      const r1 = db.prepare('SELECT * FROM m').get();
      const r2 = db.prepare('SELECT * FROM m').get();
      assert.strictEqual(r1.value, 1);
      assert.strictEqual(r2.value, 2);
      assert.strictEqual(callCount, 2);
    });
  });
});
