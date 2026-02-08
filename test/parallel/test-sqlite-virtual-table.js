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
