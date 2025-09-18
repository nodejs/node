'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { DatabaseSync, constants } = require('node:sqlite');
const { suite, it } = require('node:test');

suite('DatabaseSync.prototype.setAuthorizer()', () => {
  it('calls the authorizer with the correct parameters', (t) => {
    const authorizer = t.mock.fn(() => constants.SQLITE_OK);

    const db = new DatabaseSync(':memory:');

    db.setAuthorizer(authorizer);

    const insert = db.prepare('SELECT 1');
    insert.run();

    assert.strictEqual(authorizer.mock.callCount(), 1);

    const call = authorizer.mock.calls[0];
    assert.deepStrictEqual(call.arguments, [constants.SQLITE_SELECT, null, null, null, null]);
    assert.strictEqual(call.result, constants.SQLITE_OK);
    assert.strictEqual(call.error, undefined);
  });

  it('allows operations when authorizer returns SQLITE_OK', () => {
    const db = new DatabaseSync(':memory:');

    db.setAuthorizer(() => constants.SQLITE_OK);

    db.exec('CREATE TABLE users (id INTEGER, name TEXT)');

    const tables = db.prepare("SELECT name FROM sqlite_master WHERE type='table'").all();

    assert.strictEqual(tables[0].name, 'users');
  });

  it('blocks operations when authorizer returns SQLITE_DENY', () => {
    const db = new DatabaseSync(':memory:');

    db.setAuthorizer(() => constants.SQLITE_DENY);

    assert.throws(() => {
      db.exec('SELECT 1');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /not authorized/,
    });
  });

  it('clears authorizer with null', (t) => {
    const authorizer = t.mock.fn(() => constants.SQLITE_OK);

    const db = new DatabaseSync(':memory:');

    db.setAuthorizer(authorizer);

    const statement = db.prepare('SELECT 1');
    statement.run();

    assert.strictEqual(authorizer.mock.callCount(), 1);

    // Clear authorizer
    db.setAuthorizer(null);

    statement.run();

    assert.strictEqual(authorizer.mock.callCount(), 1);
  });

  it('throws with invalid callback type', () => {
    const db = new DatabaseSync(':memory:');

    assert.throws(() => {
      db.setAuthorizer('not a function');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/,
    });

    assert.throws(() => {
      db.setAuthorizer(1);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/,
    });
  });

});
