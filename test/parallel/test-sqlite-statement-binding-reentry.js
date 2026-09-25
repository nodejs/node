'use strict';

const { skipIfSQLiteMissing, mustCall } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { test } = require('node:test');
const { DatabaseSync } = require('node:sqlite');

const reentryError = {
  code: 'ERR_INVALID_STATE',
  message: 'statement is already being executed',
};

// Binding a named parameter reads properties off the supplied object, so a
// getter runs JavaScript after the statement has been reset but before it is
// stepped. Reentering the same statement there resets it a second time and
// hands out a second iterator over one virtual machine.
for (const method of ['all', 'get', 'run', 'iterate']) {
  test(`${method}() reentry during parameter binding is rejected`, () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE data (value INTEGER);
      INSERT INTO data VALUES (1), (2), (3);
    `);

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

test('two iterators cannot share one virtual machine', () => {
  const db = new DatabaseSync(':memory:');
  db.exec(`
    CREATE TABLE data (value INTEGER);
    INSERT INTO data VALUES (1), (2), (3);
  `);

  const statement = db.prepare('SELECT value FROM data WHERE value >= $min');
  let inner;
  const reenter = mustCall(() => {
    assert.throws(() => {
      inner = statement.iterate({ $min: 1 });
    }, reentryError);
    return 1;
  });
  const params = { get $min() { return reenter(); } };

  assert.deepStrictEqual(
    [...statement.iterate(params)].map((row) => row.value),
    [1, 2, 3],
  );
  assert.strictEqual(inner, undefined);
});
