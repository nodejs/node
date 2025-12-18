'use strict';

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

const assert = require('node:assert');
const { DatabaseSync, constants } = require('node:sqlite');
const { suite, it } = require('node:test');

suite('DatabaseSync.prototype.setAuthorizer()', () => {
  const createTestDatabase = () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE users (id INTEGER, name TEXT)');
    return db;
  };

  it('receives correct parameters for SELECT operations', (t) => {
    const authorizer = t.mock.fn(() => constants.SQLITE_OK);
    const db = createTestDatabase();

    db.setAuthorizer(authorizer);
    db.prepare('SELECT id FROM users').get();

    assert.strictEqual(authorizer.mock.callCount(), 2);
    const callArguments = authorizer.mock.calls.map((call) => call.arguments);

    assert.deepStrictEqual(
      callArguments,
      [
        [constants.SQLITE_SELECT, null, null, null, null],
        [constants.SQLITE_READ, 'users', 'id', 'main', null],
      ]
    );
  });

  it('receives correct parameters for INSERT operations', (t) => {
    const authorizer = t.mock.fn(() => constants.SQLITE_OK);
    const db = createTestDatabase();

    db.setAuthorizer(authorizer);
    db.prepare('INSERT INTO users (id, name) VALUES (?, ?)').run(1, 'node');

    assert.strictEqual(authorizer.mock.callCount(), 1);

    const callArguments = authorizer.mock.calls.map((call) => call.arguments);
    assert.deepStrictEqual(
      callArguments,
      [[constants.SQLITE_INSERT, 'users', null, 'main', null]],
    );
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
      message: /not authorized/
    });
  });

  it('ignores SELECT operations when authorizer returns SQLITE_IGNORE', () => {
    const db = createTestDatabase();
    db.prepare('INSERT INTO users (id, name) VALUES (?, ?)').run(1, 'Alice');

    db.setAuthorizer((actionCode) => {
      if (actionCode === constants.SQLITE_SELECT) {
        return constants.SQLITE_IGNORE;
      }
      return constants.SQLITE_OK;
    });

    // SELECT should be ignored and return no results
    const result = db.prepare('SELECT * FROM users').all();
    assert.deepStrictEqual(result, []);
  });

  it('ignores READ operations when authorizer returns SQLITE_IGNORE', () => {
    const db = createTestDatabase();
    db.prepare('INSERT INTO users (id, name) VALUES (?, ?)').run(1, 'Alice');

    db.setAuthorizer((actionCode, arg1, arg2) => {
      if (actionCode === constants.SQLITE_READ && arg1 === 'users' && arg2 === 'name') {
        return constants.SQLITE_IGNORE;
      }
      return constants.SQLITE_OK;
    });

    // Reading the 'name' column should be ignored, returning NULL
    const result = db.prepare('SELECT id, name FROM users WHERE id = 1').get();
    assert.strictEqual(result.id, 1);
    assert.strictEqual(result.name, null);
  });

  it('ignores INSERT operations when authorizer returns SQLITE_IGNORE', () => {
    const db = createTestDatabase();

    db.setAuthorizer((actionCode) => {
      if (actionCode === constants.SQLITE_INSERT) {
        return constants.SQLITE_IGNORE;
      }
      return constants.SQLITE_OK;
    });

    db.prepare('INSERT INTO users (id, name) VALUES (?, ?)').run(1, 'Alice');

    // Verify no data was inserted
    const count = db.prepare('SELECT COUNT(*) as count FROM users').get();
    assert.strictEqual(count.count, 0);
  });

  it('ignores UPDATE operations when authorizer returns SQLITE_IGNORE', () => {
    const db = createTestDatabase();
    db.exec("INSERT INTO users (id, name) VALUES (1, 'Alice')");

    db.setAuthorizer((actionCode) => {
      if (actionCode === constants.SQLITE_UPDATE) {
        return constants.SQLITE_IGNORE;
      }
      return constants.SQLITE_OK;
    });

    db.prepare('UPDATE users SET name = ? WHERE id = ?').run('Bob', 1);

    // Verify data was not updated
    const result = db.prepare('SELECT name FROM users WHERE id = 1').get();
    assert.strictEqual(result.name, 'Alice');
  });

  it('ignores DELETE operations when authorizer returns SQLITE_IGNORE', () => {
    const db = createTestDatabase();
    db.exec("INSERT INTO users (id, name) VALUES (1, 'Alice')");

    db.setAuthorizer(() => constants.SQLITE_IGNORE);

    db.prepare('DELETE FROM users WHERE id = ?').run(1);

    db.setAuthorizer(null);

    // Verify data was not deleted
    const count = db.prepare('SELECT COUNT(*) as count FROM users').get();
    assert.strictEqual(count.count, 1);
  });

  it('rethrows error when authorizer throws error', () => {
    const db = new DatabaseSync(':memory:');
    db.setAuthorizer(() => {
      throw new Error('Unknown error');
    });

    assert.throws(() => {
      db.exec('SELECT 1');
    }, {
      message: 'Unknown error'
    });
  });

  it('throws error when authorizer returns nothing', () => {
    const db = new DatabaseSync(':memory:');
    db.setAuthorizer(() => {
    });

    assert.throws(() => {
      db.exec('SELECT 1');
    }, {
      message: 'Authorizer callback must return an integer authorization code'
    });
  });

  it('throws error when authorizer returns NaN', () => {
    const db = new DatabaseSync(':memory:');
    db.setAuthorizer(() => {
      return '1';
    });

    assert.throws(() => {
      db.exec('SELECT 1');
    }, {
      message: 'Authorizer callback must return an integer authorization code'
    });
  });

  it('throws error when authorizer returns a invalid code', () => {
    const db = new DatabaseSync(':memory:');
    db.setAuthorizer(() => {
      return 3;
    });

    assert.throws(() => {
      db.exec('SELECT 1');
    }, {
      message: 'Authorizer callback returned a invalid authorization code'
    });
  });

  it('clears authorizer when set to null', (t) => {
    const authorizer = t.mock.fn(() => constants.SQLITE_OK);
    const db = new DatabaseSync(':memory:');
    const statement = db.prepare('SELECT 1');

    // Set authorizer and verify it's called
    db.setAuthorizer(authorizer);
    statement.run();
    assert.strictEqual(authorizer.mock.callCount(), 1);

    // Clear authorizer and verify it's no longer called
    db.setAuthorizer(null);
    statement.run();
    assert.strictEqual(authorizer.mock.callCount(), 1);
  });

  it('throws when callback is a string', () => {
    const db = new DatabaseSync(':memory:');

    assert.throws(() => {
      db.setAuthorizer('not a function');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/
    });
  });

  it('throws when callback is a number', () => {
    const db = new DatabaseSync(':memory:');

    assert.throws(() => {
      db.setAuthorizer(1);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/
    });
  });

  it('throws when callback is an object', () => {
    const db = new DatabaseSync(':memory:');

    assert.throws(() => {
      db.setAuthorizer({});
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/
    });
  });

  it('throws when callback is an array', () => {
    const db = new DatabaseSync(':memory:');

    assert.throws(() => {
      db.setAuthorizer([]);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/
    });
  });

  it('throws when callback is undefined', () => {
    const db = new DatabaseSync(':memory:');

    assert.throws(() => {
      db.setAuthorizer();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "callback" argument must be a function/
    });
  });
});
