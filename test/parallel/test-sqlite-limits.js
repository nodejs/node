'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

suite('DatabaseSync limits', () => {
  test('limits object has expected properties with positive values', (t) => {
    const db = new DatabaseSync(':memory:');
    const expectedProperties = [
      'length',
      'sqlLength',
      'column',
      'exprDepth',
      'compoundSelect',
      'vdbeOp',
      'functionArg',
      'attach',
      'likePatternLength',
      'variableNumber',
      'triggerDepth',
    ];

    for (const prop of expectedProperties) {
      t.assert.strictEqual(typeof db.limits[prop], 'number',
                           `${prop} should be a number`);
      t.assert.ok(db.limits[prop] > 0,
                  `${prop} should be positive`);
    }
  });

  test('constructor accepts limits option', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        length: 500000,
        sqlLength: 50000,
        column: 100,
        exprDepth: 50,
        compoundSelect: 10,
        vdbeOp: 10000,
        functionArg: 8,
        attach: 5,
        likePatternLength: 1000,
        variableNumber: 100,
        triggerDepth: 5,
      }
    });

    t.assert.strictEqual(db.limits.length, 500000);
    t.assert.strictEqual(db.limits.sqlLength, 50000);
    t.assert.strictEqual(db.limits.column, 100);
    t.assert.strictEqual(db.limits.exprDepth, 50);
    t.assert.strictEqual(db.limits.compoundSelect, 10);
    t.assert.strictEqual(db.limits.vdbeOp, 10000);
    t.assert.strictEqual(db.limits.functionArg, 8);
    t.assert.strictEqual(db.limits.attach, 5);
    t.assert.strictEqual(db.limits.likePatternLength, 1000);
    t.assert.strictEqual(db.limits.variableNumber, 100);
    t.assert.strictEqual(db.limits.triggerDepth, 5);
  });

  test('getter returns current limit value', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.strictEqual(typeof db.limits.length, 'number');
    t.assert.ok(db.limits.length > 0);
    t.assert.strictEqual(typeof db.limits.sqlLength, 'number');
    t.assert.ok(db.limits.sqlLength > 0);
  });

  test('setter modifies limit value', (t) => {
    const db = new DatabaseSync(':memory:');

    db.limits.length = 100000;
    t.assert.strictEqual(db.limits.length, 100000);

    db.limits.sqlLength = 50000;
    t.assert.strictEqual(db.limits.sqlLength, 50000);

    db.limits.column = 50;
    t.assert.strictEqual(db.limits.column, 50);
  });

  test('Infinity resets limit to maximum', (t) => {
    const db = new DatabaseSync(':memory:');
    const originalLength = db.limits.length;

    // Set to a lower value
    db.limits.length = 100;
    t.assert.strictEqual(db.limits.length, 100);

    // Reset to maximum using Infinity
    db.limits.length = Infinity;
    t.assert.strictEqual(db.limits.length, originalLength);
  });

  test('throws on invalid argument type', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.limits.length = 'invalid';
    }, {
      name: 'TypeError',
      message: /Limit value must be a non-negative integer or Infinity/,
    });
  });

  test('throws on negative value', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.limits.length = -1;
    }, {
      name: 'RangeError',
      message: /Limit value must be non-negative/,
    });
  });

  test('throws on null value', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.limits.length = null;
    }, {
      name: 'TypeError',
      message: /Limit value must be a non-negative integer or Infinity/,
    });
  });

  test('throws on negative Infinity', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.limits.length = -Infinity;
    }, {
      name: 'TypeError',
      message: /Limit value must be a non-negative integer or Infinity/,
    });
  });

  test('throws on getter access after close', (t) => {
    const db = new DatabaseSync(':memory:');
    db.close();
    t.assert.throws(() => {
      return db.limits.length;
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws on setter access after close', (t) => {
    const db = new DatabaseSync(':memory:');
    db.close();
    t.assert.throws(() => {
      db.limits.length = 100;
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('limits object is enumerable', (t) => {
    const db = new DatabaseSync(':memory:');
    const keys = Object.keys(db.limits);
    t.assert.ok(keys.includes('length'));
    t.assert.ok(keys.includes('sqlLength'));
    t.assert.ok(keys.includes('column'));
    t.assert.ok(keys.includes('exprDepth'));
    t.assert.ok(keys.includes('compoundSelect'));
    t.assert.ok(keys.includes('vdbeOp'));
    t.assert.ok(keys.includes('functionArg'));
    t.assert.ok(keys.includes('attach'));
    t.assert.ok(keys.includes('likePatternLength'));
    t.assert.ok(keys.includes('variableNumber'));
    t.assert.ok(keys.includes('triggerDepth'));
  });

  test('throws on invalid limits option type', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(':memory:', { limits: 'invalid' });
    }, {
      name: 'TypeError',
      message: /options\.limits.*must be an object/,
    });
  });

  test('throws on invalid limit value type in constructor', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(':memory:', { limits: { length: 'invalid' } });
    }, {
      name: 'TypeError',
      message: /options\.limits\.length.*must be an integer/,
    });
  });

  test('throws on negative limit value in constructor', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(':memory:', { limits: { length: -100 } });
    }, {
      name: 'RangeError',
      message: /options\.limits\.length.*must be non-negative/,
    });
  });

  test('throws on Infinity limit value in constructor', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(':memory:', { limits: { length: Infinity } });
    }, {
      name: 'TypeError',
      message: /options\.limits\.length.*must be an integer/,
    });
  });

  test('partial limits in constructor', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        length: 100000,
      }
    });
    t.assert.strictEqual(db.limits.length, 100000);
    t.assert.strictEqual(typeof db.limits.sqlLength, 'number');
  });

  test('throws when exceeding column limit', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        column: 10,
      }
    });

    db.exec('CREATE TABLE t1 (c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)');

    t.assert.throws(() => {
      db.exec('CREATE TABLE t2 (c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)');
    }, {
      message: /too many columns/,
    });
  });

  test('throws when exceeding attach limit', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        attach: 0,
      }
    });

    t.assert.throws(() => {
      db.exec("ATTACH DATABASE ':memory:' AS db1");
    }, {
      message: /too many attached databases/,
    });
  });

  test('throws when exceeding variable number limit', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        variableNumber: 2,
      }
    });

    t.assert.throws(() => {
      const stmt = db.prepare('SELECT ?, ?, ?');
      stmt.all(1, 2, 3);
    }, {
      message: /too many SQL variables/,
    });
  });

  test('throws when exceeding compound select limit', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        compoundSelect: 1,
      }
    });

    t.assert.throws(() => {
      db.exec('SELECT 1 UNION SELECT 2 UNION SELECT 3');
    }, {
      message: /too many terms in compound SELECT/,
    });
  });

  test('throws when exceeding function arg limit', (t) => {
    const db = new DatabaseSync(':memory:', {
      limits: {
        functionArg: 2,
      }
    });

    t.assert.throws(() => {
      db.exec('SELECT max(1, 2, 3)');
    }, {
      message: /too many arguments on function max/,
    });
  });

  test('setter applies limit to SQLite immediately', (t) => {
    const db = new DatabaseSync(':memory:');

    db.limits.attach = 0;

    t.assert.throws(() => {
      db.exec("ATTACH DATABASE ':memory:' AS db1");
    }, {
      message: /too many attached databases/,
    });
  });
});
