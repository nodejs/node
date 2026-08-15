'use strict';

const common = require('../common');
common.skipIfSQLiteMissing();

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

  it('throws if database is not open', () => {
    const db = new DatabaseSync(':memory:');
    db.close();

    assert.throws(() => {
      db.setAuthorizer(() => constants.SQLITE_OK);
    }, {
      code: 'ERR_INVALID_STATE',
      message: 'database is not open',
    });
  });
});

// SQLite forbids an authorizer callback from modifying the connection that
// invoked it, which includes preparing and stepping statements.
// See https://www.sqlite.org/c3ref/set_authorizer.html.
suite('authorizer callback reentrancy', () => {
  const expectedError = 'ERR_INVALID_STATE: database cannot be accessed ' +
    'from an authorizer callback';
  const steppingError =
    'ERR_INVALID_STATE: statement is already being executed';

  // Calls each of `cases` from inside an authorizer callback, and returns a
  // `name -> outcome` map of what each one threw.
  const runInAuthorizer = (db, cases) => {
    const outcomes = {};
    for (const [name, fn] of Object.entries(cases)) {
      let ran = false;
      db.setAuthorizer(() => {
        if (!ran) {
          ran = true;
          try {
            fn();
            outcomes[name] = 'did not throw';
          } catch (err) {
            outcomes[name] = `${err.code}: ${err.message}`;
          }
        }
        return constants.SQLITE_OK;
      });
      db.exec('SELECT 1');
      db.setAuthorizer(null);
      if (!ran) {
        outcomes[name] = 'authorizer callback did not run';
      }
    }
    return outcomes;
  };

  // Builds the expected `name -> outcome` map for the given case names.
  const allRejected = (cases) => Object.fromEntries(
    Object.keys(cases).map((name) => [name, expectedError]),
  );

  it('rejects database methods', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    const cases = {
      prepare: () => db.prepare('SELECT 1'),
      exec: () => db.exec('SELECT 1'),
      setAuthorizer: () => db.setAuthorizer(null),
      createSession: () => db.createSession(),
      applyChangeset: () => db.applyChangeset(new Uint8Array([1])),
      createTagStore: () => db.createTagStore(),
      serialize: () => db.serialize(),
      function: () => db.function('noop', () => 1),
      aggregate: () => db.aggregate('agg', { start: 0, step: (acc) => acc }),
      enableLoadExtension: () => db.enableLoadExtension(false),
      enableDefensive: () => db.enableDefensive(true),
      limits: () => { db.limits.length = 100; },
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));
  });

  // loadExtension() checks that extension loading is enabled before reaching
  // the authorizer guard, so it needs a database opened with allowExtension.
  it('rejects loadExtension', () => {
    const db = new DatabaseSync(':memory:', { allowExtension: true });
    db.enableLoadExtension(true);
    db.exec('CREATE TABLE t (x INTEGER)');
    const cases = {
      loadExtension: () => db.loadExtension('/nonexistent/extension'),
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));
  });

  // close() and deserialize() tear down the connection, so the pre-existing
  // callback depth guard already rejects them with its own message.
  it('rejects methods the callback depth guard already covers', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    const snapshot = db.serialize();
    const cases = {
      close: () => db.close(),
      deserialize: () => db.deserialize(snapshot),
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), {
      close: 'ERR_INVALID_STATE: database cannot be closed while in a callback',
      deserialize: 'ERR_INVALID_STATE: database cannot be deserialized ' +
        'while in a callback',
    });
  });

  it('rejects statement methods', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const stmt = db.prepare('SELECT x FROM t');
    const cases = {
      run: () => stmt.run(),
      get: () => stmt.get(),
      all: () => stmt.all(),
      iterate: () => stmt.iterate(),
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));
  });

  // Only the statement being stepped is unsafe to finalize. Other statements
  // on the connection have their own virtual machines, so finalizing them from
  // a callback is allowed.
  it('allows finalizing a statement that is not being executed', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const closeStmt = db.prepare('SELECT x FROM t');
    const disposeStmt = db.prepare('SELECT x FROM t');
    const cases = {
      close: () => closeStmt.close(),
      dispose: () => disposeStmt[Symbol.dispose](),
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), {
      close: 'did not throw',
      dispose: 'did not throw',
    });
  });

  // Disposal is idempotent, so a statement that is already finalized must stay
  // a no-op even inside a callback. Throwing here would turn a `using` scope's
  // real exception into a SuppressedError.
  it('allows disposing an already-finalized statement', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    const stmt = db.prepare('SELECT x FROM t');
    stmt.close();
    const cases = { dispose: () => stmt[Symbol.dispose]() };

    assert.deepStrictEqual(runInAuthorizer(db, cases), {
      dispose: 'did not throw',
    });
  });

  it('rejects session changeset methods', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER PRIMARY KEY, y TEXT)');
    const session = db.createSession({ table: 't' });
    db.exec("INSERT INTO t VALUES (1, 'a')");
    const cases = {
      changeset: () => session.changeset(),
      patchset: () => session.patchset(),
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));
  });

  // A statement being re-prepared inside sqlite3_step() is the case that
  // actually crashes, because that statement's VM is mid-execution.
  it('rejects finalizing the statement being stepped', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const stmt = db.prepare('SELECT x FROM t');
    stmt.get();
    db.exec('ALTER TABLE t ADD COLUMN y INTEGER');

    let outcome = 'authorizer callback did not run';
    let ran = false;
    db.setAuthorizer(() => {
      if (!ran) {
        ran = true;
        try {
          stmt.close();
          outcome = 'did not throw';
        } catch (err) {
          outcome = `${err.code}: ${err.message}`;
        }
      }
      return constants.SQLITE_OK;
    });

    stmt.get();

    assert.strictEqual(outcome, steppingError);
  });

  // Unlike an already-finalized statement, disposing the one being stepped
  // would free the running virtual machine, so it throws.
  it('rejects disposing the statement being stepped', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const stmt = db.prepare('SELECT x FROM t');
    stmt.get();
    db.exec('ALTER TABLE t ADD COLUMN y INTEGER');

    let outcome = 'authorizer callback did not run';
    let ran = false;
    db.setAuthorizer(() => {
      if (!ran) {
        ran = true;
        try {
          stmt[Symbol.dispose]();
          outcome = 'did not throw';
        } catch (err) {
          outcome = `${err.code}: ${err.message}`;
        }
      }
      return constants.SQLITE_OK;
    });

    stmt.get();

    assert.strictEqual(outcome, steppingError);
  });

  it('rejects iterator methods', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1), (2)');
    const iter = db.prepare('SELECT x FROM t').iterate();
    const cases = {
      next: () => iter.next(),
      return: () => iter.return(),
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));
    iter.return();
  });

  // A drained iterator holds no SQLite state, so next() and return() stay
  // available and remain idempotent inside a callback.
  it('allows iterator methods on a drained iterator', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const iter = db.prepare('SELECT x FROM t').iterate();
    for (const row of iter) {
      assert.ok(row);
    }
    const done = {};
    const cases = {
      next: () => { done.next = iter.next().done; },
      return: () => { done.return = iter.return().done; },
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), {
      next: 'did not throw',
      return: 'did not throw',
    });
    assert.deepStrictEqual(done, { next: true, return: true });
  });

  it('rejects tag store methods', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    const sql = db.createTagStore(10);
    const cases = {
      run: () => sql.run`SELECT 1`,
      get: () => sql.get`SELECT 1`,
      all: () => sql.all`SELECT 1`,
      iterate: () => sql.iterate`SELECT 1`,
    };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));
  });

  // clear() only drops cached statements, so invalidating the cache after a
  // schema change is allowed from the callback.
  it('allows clearing a tag store', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const sql = db.createTagStore(10);
    assert.strictEqual(sql.all`SELECT x FROM t`.length, 1);
    assert.strictEqual(sql.size, 1);
    const cases = { clear: () => sql.clear() };

    assert.deepStrictEqual(runInAuthorizer(db, cases), {
      clear: 'did not throw',
    });
    assert.strictEqual(sql.size, 0);
  });

  // A statement may be re-prepared during sqlite3_step() after a schema
  // change, which invokes the authorizer without an explicit prepare() call.
  it('rejects reentry when the authorizer runs during a re-prepare', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    const stmt = db.prepare('SELECT x FROM t');
    stmt.get();
    db.exec('ALTER TABLE t ADD COLUMN y INTEGER');

    let outcome = 'authorizer callback did not run';
    let ran = false;
    db.setAuthorizer(() => {
      if (!ran) {
        ran = true;
        try {
          db.prepare('SELECT 1');
          outcome = 'did not throw';
        } catch (err) {
          outcome = `${err.code}: ${err.message}`;
        }
      }
      return constants.SQLITE_OK;
    });

    stmt.get();

    assert.strictEqual(outcome, expectedError);
  });

  it('allows access again after the authorizer returns', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t (x INTEGER)');
    const cases = { prepare: () => db.prepare('SELECT 1') };

    assert.deepStrictEqual(runInAuthorizer(db, cases), allRejected(cases));

    db.setAuthorizer(() => constants.SQLITE_OK);
    assert.deepStrictEqual(db.prepare('SELECT 1 AS v').get(), { __proto__: null, v: 1 });
  });
});
