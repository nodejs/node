import '../common/index.mjs';
import { DatabaseSync } from 'node:sqlite';
import { describe, test } from 'node:test';

describe('DatabaseSync.prototype.aggregate()', () => {
  describe('input validation', () => {
    const db = new DatabaseSync(':memory:');

    test('throws if options.start is not provided', (t) => {
      t.assert.throws(() => {
        db.aggregate('sum', {
          result: (total) => total
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "options.start" argument must be a function or a primitive value.'
      });
    });

    test('throws if options.step is not a function', (t) => {
      t.assert.throws(() => {
        db.aggregate('sum', {
          start: 0,
          result: (total) => total
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "options.step" argument must be a function.'
      });
    });

    test('throws if options.useBigIntArguments is not a boolean', (t) => {
      t.assert.throws(() => {
        db.aggregate('sum', {
          start: 0,
          step: () => null,
          useBigIntArguments: ''
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.useBigIntArguments" argument must be a boolean/,
      });
    });

    test('throws if options.varargs is not a boolean', (t) => {
      t.assert.throws(() => {
        db.aggregate('sum', {
          start: 0,
          step: () => null,
          varargs: ''
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.varargs" argument must be a boolean/,
      });
    });

    test('throws if options.directOnly is not a boolean', (t) => {
      t.assert.throws(() => {
        db.aggregate('sum', {
          start: 0,
          step: () => null,
          directOnly: ''
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.directOnly" argument must be a boolean/,
      });
    });
  });
});

describe('varargs', () => {
  test('supports variable number of arguments when true', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('sum_int', {
      start: 0,
      step: (_acc, _value, var1, var2, var3) => {
        return var1 + var2 + var3;
      },
      varargs: true,
    });

    const result = db.prepare('SELECT sum_int(value, 1, 2, 3) as total FROM data').get();

    t.assert.deepStrictEqual(result, { __proto__: null, total: 6 });
  });

  test('uses the max between step.length and inverse.length when false', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec(`
      CREATE TABLE t3(x, y);
      INSERT INTO t3 VALUES ('a', 1),
                            ('b', 2),
                            ('c', 3);
    `);

    db.aggregate('sumint', {
      start: 0,
      step: (acc, var1) => {
        return var1 + acc;
      },
      inverse: (acc, var1, var2) => {
        return acc - var1 - var2;
      },
      varargs: false,
    });

    const result = db.prepare(`
      SELECT x, sumint(y, 10) OVER (
        ORDER BY x ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
      ) AS sum_y
      FROM t3 ORDER BY x;
    `).all();

    t.assert.deepStrictEqual(result, [
      { __proto__: null, x: 'a', sum_y: 3 },
      { __proto__: null, x: 'b', sum_y: 6 },
      { __proto__: null, x: 'c', sum_y: -5 },
    ]);

    t.assert.throws(() => {
      db.prepare(`
        SELECT x, sumint(y) OVER (
          ORDER BY x ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
        ) AS sum_y
        FROM t3 ORDER BY x;
      `);
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'wrong number of arguments to function sumint()'
    });
  });

  test('throws if an incorrect number of arguments is provided when false', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.aggregate('sum_int', {
      start: 0,
      step: (_acc, var1, var2, var3) => {
        return var1 + var2 + var3;
      },
      varargs: false,
    });

    t.assert.throws(() => {
      db.prepare('SELECT sum_int(1, 2, 3, 4)').get();
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'wrong number of arguments to function sum_int()'
    });
  });
});

describe('directOnly', () => {
  test('is false by default', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.aggregate('func', {
      start: 0,
      step: (acc, value) => acc + value,
      inverse: (acc, value) => acc - value,
    });
    db.exec(`
      CREATE TABLE t3(x, y);
      INSERT INTO t3 VALUES ('a', 4),
                            ('b', 5),
                            ('c', 3);
    `);

    db.exec(`
      CREATE TRIGGER test_trigger
      AFTER INSERT ON t3
      BEGIN
          SELECT func(1) OVER ();
      END;
    `);

    // TRIGGER will work fine with the window function
    db.exec('INSERT INTO t3 VALUES(\'d\', 6)');
  });

  test('set SQLITE_DIRECT_ONLY flag when true', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.aggregate('func', {
      start: 0,
      step: (acc, value) => acc + value,
      inverse: (acc, value) => acc - value,
      directOnly: true,
    });
    db.exec(`
      CREATE TABLE t3(x, y);
      INSERT INTO t3 VALUES ('a', 4),
                            ('b', 5),
                            ('c', 3);
    `);

    db.exec(`
      CREATE TRIGGER test_trigger
      AFTER INSERT ON t3
      BEGIN
          SELECT func(1) OVER ();
      END;
    `);

    t.assert.throws(() => {
      db.exec('INSERT INTO t3 VALUES(\'d\', 6)');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /unsafe use of func\(\)/
    });
  });
});

describe('start', () => {
  test('start option as a value', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('sum_int', {
      start: 0,
      step: (acc, value) => acc + value,
    });

    const result = db.prepare('SELECT sum_int(value) as total FROM data').get();

    t.assert.deepStrictEqual(result, { __proto__: null, total: 6 });
  });

  test('start option as a function', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('sum_int', {
      start: () => 0,
      step: (acc, value) => acc + value,
    });

    const result = db.prepare('SELECT sum_int(value) as total FROM data').get();

    t.assert.deepStrictEqual(result, { __proto__: null, total: 6 });
  });

  test('start option can hold any js value', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('sum_int', {
      start: () => [],
      step: (acc, value) => {
        return [...acc, value];
      },
      result: (acc) => acc.join(', '),
    });

    const result = db.prepare('SELECT sum_int(value) as total FROM data').get();

    t.assert.deepStrictEqual(result, { __proto__: null, total: '1, 2, 3' });
  });

  test('throws if start throws an error', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('agg', {
      start: () => {
        throw new Error('start error');
      },
      step: () => null,
    });

    t.assert.throws(() => {
      db.prepare('SELECT agg()').get();
    }, {
      message: 'start error'
    });
  });
});

describe('step', () => {
  test('throws if step throws an error', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('agg', {
      start: 0,
      step: () => {
        throw new Error('step error');
      },
    });

    t.assert.throws(() => {
      db.prepare('SELECT agg()').get();
    }, {
      message: 'step error'
    });
  });
});

describe('result', () => {
  test('executes once when options.inverse is not present', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    const mockFn = t.mock.fn(() => 'overridden');
    db.exec('CREATE TABLE data (value INTEGER)');
    db.exec('INSERT INTO data VALUES (1), (2), (3)');
    db.aggregate('sum_int', {
      start: 0,
      step: (acc, value) => {
        return acc + value;
      },
      result: mockFn
    });

    const result = db.prepare('SELECT sum_int(value) as result FROM data').get();

    t.assert.deepStrictEqual(result, { __proto__: null, result: 'overridden' });
    t.assert.strictEqual(mockFn.mock.calls.length, 1);
    t.assert.deepStrictEqual(mockFn.mock.calls[0].arguments, [6]);
  });

  test('executes once per row when options.inverse is present', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    const mockFn = t.mock.fn((acc) => acc);
    db.exec(`
      CREATE TABLE t3(x, y);
      INSERT INTO t3 VALUES ('a', 4),
                            ('b', 5),
                            ('c', 3);
    `);
    db.aggregate('sumint', {
      start: 0,
      step: (acc, value) => {
        return acc + value;
      },
      inverse: (acc, value) => {
        return acc - value;
      },
      result: mockFn
    });

    db.prepare(`
      SELECT x, sumint(y) OVER (
        ORDER BY x ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
      ) AS sum_y
      FROM t3 ORDER BY x;
    `).all();

    t.assert.strictEqual(mockFn.mock.calls.length, 3);
    t.assert.deepStrictEqual(mockFn.mock.calls[0].arguments, [9]);
    t.assert.deepStrictEqual(mockFn.mock.calls[1].arguments, [12]);
    t.assert.deepStrictEqual(mockFn.mock.calls[2].arguments, [8]);
  });
});

test('throws an error when trying to use as windown function but didn\'t provide options.inverse', (t) => {
  const db = new DatabaseSync(':memory:');
  t.after(() => db.close());
  db.exec(`
    CREATE TABLE t3(x, y);
    INSERT INTO t3 VALUES ('a', 4),
                          ('b', 5),
                          ('c', 3);
  `);

  db.aggregate('sumint', {
    start: 0,
    step: (total, nextValue) => total + nextValue,
  });

  t.assert.throws(() => {
    db.prepare(`
      SELECT x, sumint(y) OVER (
        ORDER BY x ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
      ) AS sum_y
      FROM t3 ORDER BY x;
    `);
  }, {
    code: 'ERR_SQLITE_ERROR',
    message: 'sumint() may not be used as a window function'
  });
});
