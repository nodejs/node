'use strict';

const { skipIfSQLiteMissing, mustCall } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { suite, test } = require('node:test');
const { DatabaseSync } = require('node:sqlite');

const reentryError = {
  code: 'ERR_INVALID_STATE',
  message: 'statement is currently being executed',
};

function newDbWithRows() {
  const db = new DatabaseSync(':memory:');
  db.exec(`
    CREATE TABLE data (value INTEGER);
    INSERT INTO data VALUES (1), (2), (3);
  `);
  return db;
}

suite('reentry into the running statement is rejected', () => {
  for (const method of ['all', 'get', 'run']) {
    test(`statement.${method}() from its own UDF`, () => {
      const db = newDbWithRows();
      let statement;
      db.function('reenter', mustCall((value) => {
        assert.throws(() => statement[method](), reentryError);
        return value;
      }));

      statement = db.prepare('SELECT reenter(value) AS value FROM data LIMIT 1');
      assert.deepStrictEqual(statement.get(), { __proto__: null, value: 1 });
      assert.strictEqual(db.isOpen, true);
    });
  }

  test('iterator next() from its own UDF', () => {
    const db = newDbWithRows();
    let iterator;
    db.function('reenter', mustCall((value) => {
      assert.throws(() => iterator.next(), reentryError);
      return value;
    }, 3));

    iterator = db.prepare('SELECT reenter(value) AS value FROM data').iterate();
    assert.deepStrictEqual([...iterator].map((row) => row.value), [1, 2, 3]);
    assert.strictEqual(db.isOpen, true);
  });

  test('iterator return() from its own UDF', () => {
    const db = newDbWithRows();
    let iterator;
    db.function('reenter', mustCall((value) => {
      assert.throws(() => iterator.return(), reentryError);
      return value;
    }));

    iterator = db.prepare('SELECT reenter(value) AS value FROM data').iterate();
    assert.strictEqual(iterator.next().done, false);
    iterator.return();
    assert.strictEqual(db.isOpen, true);
  });

  test(
    'recursive get() reports the reentry rather than overflowing the stack',
    () => {
      const db = new DatabaseSync(':memory:');
      let statement;
      db.function('reenter', mustCall(() => {
        assert.throws(() => statement.get(), reentryError);
        return 1;
      }));

      statement = db.prepare('SELECT reenter() AS value');
      assert.deepStrictEqual(statement.get(), { __proto__: null, value: 1 });
    });

  for (const method of ['all', 'get', 'run', 'iterate']) {
    test(`${method}() reentry during parameter binding is rejected`, () => {
      const db = newDbWithRows();
      let statement;
      const invoke = (params) => (method === 'iterate' ?
        [...statement.iterate(params)] :
        statement[method](params));
      const reenter = mustCall(() => {
        assert.throws(() => invoke({ $min: 2 }), reentryError);
        return 1;
      });
      const params = { get $min() { return reenter(); } };

      statement = db.prepare('SELECT value FROM data WHERE value >= $min');
      invoke(params);
    });
  }

  test('statement.close() from its own UDF', () => {
    const db = newDbWithRows();
    let statement;
    db.function('reenter', mustCall((value) => {
      assert.throws(() => statement.close(), reentryError);
      return value;
    }));

    statement = db.prepare('SELECT reenter(value) AS value FROM data LIMIT 1');
    assert.deepStrictEqual(statement.get(), { __proto__: null, value: 1 });
    statement.close();
  });

  test('statement[Symbol.dispose]() from its own UDF', () => {
    const db = newDbWithRows();
    let statement;
    db.function('reenter', mustCall((value) => {
      assert.throws(() => statement[Symbol.dispose](), reentryError);
      return value;
    }));

    statement = db.prepare('SELECT reenter(value) AS value FROM data LIMIT 1');
    assert.deepStrictEqual(statement.get(), { __proto__: null, value: 1 });
    statement[Symbol.dispose]();
  });

  test('statement is usable again after the callback returns', () => {
    const db = newDbWithRows();
    let statement;
    db.function('reenter', mustCall((value) => {
      assert.throws(() => statement.all(), reentryError);
      return value;
    }, 2));

    statement = db.prepare('SELECT reenter(value) AS value FROM data LIMIT 1');
    assert.deepStrictEqual(statement.all(), [{ __proto__: null, value: 1 }]);
    assert.deepStrictEqual(statement.all(), [{ __proto__: null, value: 1 }]);
  });
});

suite('a different statement remains usable from a callback', () => {
  test('the lookup pattern still works', () => {
    const db = newDbWithRows();
    db.exec('CREATE TABLE names (value INTEGER, name TEXT);' +
            "INSERT INTO names VALUES (1, 'one'), (2, 'two'), (3, 'three');");
    const lookup = db.prepare('SELECT name FROM names WHERE value = ?');

    db.function('name_of', mustCall((value) => lookup.get(value).name, 3));

    assert.deepStrictEqual(
      db.prepare('SELECT name_of(value) AS name FROM data').all(),
      [
        { __proto__: null, name: 'one' },
        { __proto__: null, name: 'two' },
        { __proto__: null, name: 'three' },
      ],
    );
  });

  test('a nested iterator over a different statement still works', () => {
    const db = newDbWithRows();
    const inner = db.prepare('SELECT value FROM data');

    db.function('sum_all', mustCall(() => {
      let total = 0;
      for (const row of inner.iterate()) {
        total += row.value;
      }
      return total;
    }));

    assert.deepStrictEqual(
      db.prepare('SELECT sum_all() AS total LIMIT 1').get(),
      { __proto__: null, total: 6 },
    );
  });
});

suite('SQL tag store reentry is rejected', () => {
  for (const method of ['all', 'get', 'run']) {
    test(`tag store ${method} re-executing the same tag`, () => {
      const db = newDbWithRows();
      const sql = db.createTagStore(4);
      db.function('reenter', mustCall((value) => {
        assert.throws(
          () => sql[method]`SELECT reenter(value) AS value FROM data LIMIT 1`,
          reentryError,
        );
        return value;
      }));

      assert.deepStrictEqual(
        sql.get`SELECT reenter(value) AS value FROM data LIMIT 1`,
        { __proto__: null, value: 1 },
      );
      assert.strictEqual(db.isOpen, true);
    });
  }
});

suite('aggregate functions', () => {
  test('reentry from an aggregate step is rejected', () => {
    const db = newDbWithRows();
    let statement;
    db.aggregate('reenter_agg', {
      start: 0,
      step: mustCall((total, value) => {
        assert.throws(() => statement.get(), reentryError);
        return total + value;
      }, 3),
    });

    statement = db.prepare('SELECT reenter_agg(value) AS total FROM data');
    assert.deepStrictEqual(statement.get(), { __proto__: null, total: 6 });
  });

  test('reentry from an aggregate result is rejected', () => {
    const db = newDbWithRows();
    let statement;
    db.aggregate('reenter_result', {
      start: 0,
      step: (total, value) => total + value,
      result: mustCall((total) => {
        assert.throws(() => statement.get(), reentryError);
        return total;
      }),
    });

    statement = db.prepare('SELECT reenter_result(value) AS total FROM data');
    assert.deepStrictEqual(statement.get(), { __proto__: null, total: 6 });
  });
});
