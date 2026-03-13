'use strict';

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

const assert = require('node:assert');
const { DatabaseSync } = require('node:sqlite');
const { suite, it } = require('node:test');

suite('DatabaseSync verbose option', () => {
  it('callback receives SQL string for exec() statements', (t) => {
    const calls = [];
    const db = new DatabaseSync(':memory:', {
      verbose: (sql) => calls.push(sql),
    });

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');

    assert.strictEqual(calls.length, 2);
    assert.strictEqual(calls[0], 'CREATE TABLE t (x INTEGER)');
    assert.strictEqual(calls[1], 'INSERT INTO t VALUES (1)');
    db.close();
  });

  it('callback receives SQL string for prepared statement execution', (t) => {
    let calls = [];
    const db = new DatabaseSync(':memory:', {
      verbose: (sql) => calls.push(sql),
    });

    db.exec('CREATE TABLE t (x INTEGER)');
    calls = []; // reset after setup

    const stmt = db.prepare('INSERT INTO t VALUES (?)');
    stmt.run(42);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0], 'INSERT INTO t VALUES (42.0)');
    db.close();
  });

  it('falls back to source SQL when expansion fails', () => {
    let calls = [];

    const db = new DatabaseSync(':memory:', {
      verbose: (sql) => calls.push(sql),
      limits: { length: 1000 },
    });

    db.exec('CREATE TABLE t (x TEXT)');
    calls = []; // reset after setup

    const stmt = db.prepare('INSERT INTO t VALUES (?)');

    const longValue = 'a'.repeat(977);
    stmt.run(longValue);

    assert.strictEqual(calls.length, 1);
    // Falls back to source SQL with unexpanded '?' placeholder
    assert.strictEqual(calls[0], 'INSERT INTO t VALUES (?)');
    db.close();
  });

  it('invalid type for verbose throws ERR_INVALID_ARG_TYPE', () => {
    assert.throws(() => {
      new DatabaseSync(':memory:', { verbose: 'not-a-function' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.verbose" argument must be a function\./,
    });

    assert.throws(() => {
      new DatabaseSync(':memory:', { verbose: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.verbose" argument must be a function\./,
    });
  });
});
