'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const dc = require('node:diagnostics_channel');
const { Database, constants } = require('node:sqlite');
const { suite, test } = require('node:test');

function createDb() {
  const db = new Database(':memory:');
  db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY) STRICT;');
  return db;
}

function keys(db) {
  return db.prepare('SELECT key FROM data ORDER BY key').all()
    .map((row) => row.key);
}

suite('manual transactions', () => {
  test('a transaction is committed', (t) => {
    const db = new Database(':memory:');
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(
        key INTEGER PRIMARY KEY
      ) STRICT;
    `);
    t.assert.strictEqual(setup, undefined);
    t.assert.deepStrictEqual(
      db.prepare('BEGIN').run(),
      { changes: 0, lastInsertRowid: 0 },
    );
    t.assert.deepStrictEqual(
      db.prepare('INSERT INTO data (key) VALUES (100)').run(),
      { changes: 1, lastInsertRowid: 100 },
    );
    t.assert.deepStrictEqual(
      db.prepare('COMMIT').run(),
      { changes: 1, lastInsertRowid: 100 },
    );
    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM data').all(),
      [{ __proto__: null, key: 100 }],
    );
  });

  test('a transaction is rolled back', (t) => {
    const db = new Database(':memory:');
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(
        key INTEGER PRIMARY KEY
      ) STRICT;
    `);
    t.assert.strictEqual(setup, undefined);
    t.assert.deepStrictEqual(
      db.prepare('BEGIN').run(),
      { changes: 0, lastInsertRowid: 0 },
    );
    t.assert.deepStrictEqual(
      db.prepare('INSERT INTO data (key) VALUES (100)').run(),
      { changes: 1, lastInsertRowid: 100 },
    );
    t.assert.deepStrictEqual(
      db.prepare('ROLLBACK').run(),
      { changes: 1, lastInsertRowid: 100 },
    );
    t.assert.deepStrictEqual(db.prepare('SELECT * FROM data').all(), []);
  });
});

suite('database.transaction()', () => {
  test('commits and returns the value returned by fn', (t) => {
    using db = createDb();
    const result = db.transaction(() => {
      t.assert.strictEqual(db.isTransaction, true);
      db.exec('INSERT INTO data (key) VALUES (1)');
      return 'done';
    });
    t.assert.strictEqual(result, 'done');
    t.assert.strictEqual(db.isTransaction, false);
    t.assert.deepStrictEqual(keys(db), [1]);
  });

  test('calls fn with no arguments and an undefined receiver', (t) => {
    using db = createDb();
    db.transaction(function(...args) {
      t.assert.strictEqual(this, undefined);
      t.assert.strictEqual(args.length, 0);
    }, {});
  });

  test('rolls back and rethrows the error thrown by fn', (t) => {
    using db = createDb();
    const err = new Error('boom');
    t.assert.throws(() => {
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        throw err;
      });
    }, (thrown) => thrown === err);
    t.assert.strictEqual(db.isTransaction, false);
    t.assert.deepStrictEqual(keys(db), []);
  });

  test('rolls back and rethrows a thrown primitive', (t) => {
    using db = createDb();
    t.assert.throws(() => {
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        throw 'boom';  // eslint-disable-line no-throw-literal
      });
    }, (thrown) => thrown === 'boom');
    t.assert.strictEqual(db.isTransaction, false);
    t.assert.deepStrictEqual(keys(db), []);
  });

  test('rolls back when a statement fails inside fn', (t) => {
    using db = createDb();
    t.assert.throws(() => {
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        db.exec('INSERT INTO data (key) VALUES (1)');
      });
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /UNIQUE constraint failed/,
    });
    t.assert.strictEqual(db.isTransaction, false);
    t.assert.deepStrictEqual(keys(db), []);
  });

  suite('options.mode', () => {
    for (const mode of ['deferred', 'immediate', 'exclusive']) {
      test(`supports '${mode}'`, (t) => {
        using db = createDb();
        const statements = [];
        const handler = ({ sql }) => statements.push(sql);
        dc.subscribe('sqlite.db.query', handler);
        t.after(() => dc.unsubscribe('sqlite.db.query', handler));

        db.transaction(() => {}, { mode });
        t.assert.deepStrictEqual(statements, [
          `BEGIN ${mode.toUpperCase()}`,
          'COMMIT',
        ]);
      });
    }
  });

  suite('argument validation', () => {
    test('throws if fn is not a function', (t) => {
      using db = createDb();
      for (const fn of [undefined, null, 'BEGIN', {}]) {
        t.assert.throws(() => {
          db.transaction(fn);
        }, {
          code: 'ERR_INVALID_ARG_TYPE',
          message: /The "fn" argument must be a function/,
        });
      }
      t.assert.strictEqual(db.isTransaction, false);
    });

    test('throws if options is not an object', (t) => {
      using db = createDb();
      const fn = t.mock.fn();
      for (const options of [null, 'immediate', 1]) {
        t.assert.throws(() => {
          db.transaction(fn, options);
        }, {
          code: 'ERR_INVALID_ARG_TYPE',
          message: /The "options" argument must be an object/,
        });
      }
      t.assert.strictEqual(fn.mock.callCount(), 0);
    });

    test('throws if options.mode is not a string', (t) => {
      using db = createDb();
      const fn = t.mock.fn();
      t.assert.throws(() => {
        db.transaction(fn, { mode: 1 });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.mode" argument must be a string/,
      });
      t.assert.strictEqual(fn.mock.callCount(), 0);
    });

    test('throws if options.mode is not a known mode', (t) => {
      using db = createDb();
      const fn = t.mock.fn();
      t.assert.throws(() => {
        db.transaction(fn, { mode: 'IMMEDIATE' });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /The "options\.mode" argument must be one of/,
      });
      t.assert.strictEqual(fn.mock.callCount(), 0);
      t.assert.strictEqual(db.isTransaction, false);
    });

    test('throws if the database is not open', (t) => {
      using db = new Database(':memory:', { open: false });
      const fn = t.mock.fn();
      t.assert.throws(() => {
        db.transaction(fn);
      }, {
        code: 'ERR_INVALID_STATE',
        message: /database is not open/,
      });
      t.assert.strictEqual(fn.mock.callCount(), 0);
    });
  });

  suite('nesting', () => {
    test('an inner failure only rolls back the inner changes', (t) => {
      using db = createDb();
      const err = new Error('inner');
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        t.assert.throws(() => {
          db.transaction(() => {
            db.exec('INSERT INTO data (key) VALUES (2)');
            throw err;
          });
        }, (thrown) => thrown === err);
        t.assert.strictEqual(db.isTransaction, true);
        db.exec('INSERT INTO data (key) VALUES (3)');
      });
      t.assert.strictEqual(db.isTransaction, false);
      t.assert.deepStrictEqual(keys(db), [1, 3]);
    });

    test('an outer failure rolls back committed inner changes', (t) => {
      using db = createDb();
      t.assert.throws(() => {
        db.transaction(() => {
          const result = db.transaction(() => {
            db.exec('INSERT INTO data (key) VALUES (1)');
            return 'inner';
          });
          t.assert.strictEqual(result, 'inner');
          t.assert.deepStrictEqual(keys(db), [1]);
          throw new Error('outer');
        });
      }, { message: 'outer' });
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('supports multiple levels', (t) => {
      using db = createDb();
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (2)');
          t.assert.throws(() => {
            db.transaction(() => {
              db.exec('INSERT INTO data (key) VALUES (3)');
              throw new Error('innermost');
            });
          }, { message: 'innermost' });
          db.exec('INSERT INTO data (key) VALUES (4)');
        });
      });
      t.assert.deepStrictEqual(keys(db), [1, 2, 4]);
    });

    test('nests inside a manually started transaction', (t) => {
      using db = createDb();
      db.exec('BEGIN');
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
      });
      t.assert.strictEqual(db.isTransaction, true);
      t.assert.throws(() => {
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (2)');
          throw new Error('boom');
        });
      }, { message: 'boom' });
      t.assert.strictEqual(db.isTransaction, true);
      t.assert.deepStrictEqual(keys(db), [1]);
      db.exec('ROLLBACK');
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('ignores options.mode when nested', (t) => {
      using db = createDb();
      db.transaction(() => {
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (1)');
        }, { mode: 'exclusive' });
      });
      t.assert.deepStrictEqual(keys(db), [1]);
    });
  });

  suite('Promise return values', () => {
    const invalidReturnValue = {
      code: 'ERR_INVALID_RETURN_VALUE',
      name: 'TypeError',
      message: /Expected a non-Promise value to be returned from the "fn" function/,
    };

    test('rolls back and throws for an async function', (t) => {
      using db = createDb();
      t.assert.throws(() => {
        db.transaction(async () => {
          db.exec('INSERT INTO data (key) VALUES (1)');
        });
      }, invalidReturnValue);
      t.assert.strictEqual(db.isTransaction, false);
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('rolls back and throws for a returned Promise', (t) => {
      using db = createDb();
      t.assert.throws(() => {
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (1)');
          return Promise.resolve();
        });
      }, invalidReturnValue);
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('only rolls back the nested changes when nested', (t) => {
      using db = createDb();
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        t.assert.throws(() => {
          db.transaction(async () => {
            db.exec('INSERT INTO data (key) VALUES (2)');
          });
        }, invalidReturnValue);
        t.assert.strictEqual(db.isTransaction, true);
      });
      t.assert.deepStrictEqual(keys(db), [1]);
    });
  });

  suite('fn ends the transaction', () => {
    test('does not commit again after a manual COMMIT', (t) => {
      using db = createDb();
      const result = db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        db.exec('COMMIT');
        return 'done';
      });
      t.assert.strictEqual(result, 'done');
      t.assert.strictEqual(db.isTransaction, false);
      t.assert.deepStrictEqual(keys(db), [1]);
    });

    test('does not commit after a manual ROLLBACK', (t) => {
      using db = createDb();
      db.transaction(() => {
        db.exec('INSERT INTO data (key) VALUES (1)');
        db.exec('ROLLBACK');
      });
      t.assert.strictEqual(db.isTransaction, false);
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('rethrows the original error after a manual ROLLBACK', (t) => {
      using db = createDb();
      const err = new Error('boom');
      t.assert.throws(() => {
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (1)');
          db.exec('ROLLBACK');
          throw err;
        });
      }, (thrown) => thrown === err);
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('throws if fn closes the database', (t) => {
      using db = new Database(':memory:');
      t.assert.throws(() => {
        db.transaction(() => {
          db.close();
        });
      }, {
        code: 'ERR_INVALID_STATE',
        message: /database is not open/,
      });
      t.assert.strictEqual(db.isOpen, false);
    });

    test('rethrows the original error if fn closes the database', (t) => {
      using db = new Database(':memory:');
      const err = new Error('boom');
      t.assert.throws(() => {
        db.transaction(() => {
          db.close();
          throw err;
        });
      }, (thrown) => thrown === err);
    });
  });

  test('rolls back when the commit fails', (t) => {
    using db = new Database(':memory:');
    db.exec(`
      CREATE TABLE parent(id INTEGER PRIMARY KEY) STRICT;
      CREATE TABLE child(
        id INTEGER PRIMARY KEY,
        parent_id INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED
      ) STRICT;
    `);
    t.assert.throws(() => {
      db.transaction(() => {
        db.exec('INSERT INTO child (id, parent_id) VALUES (1, 1)');
      });
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /FOREIGN KEY constraint failed/,
    });
    t.assert.strictEqual(db.isTransaction, false);
    t.assert.deepStrictEqual(db.prepare('SELECT * FROM child').all(), []);
  });

  test('commits while a read iterator is still open', (t) => {
    using db = createDb();
    db.exec('INSERT INTO data (key) VALUES (1), (2)');
    const iterator = db.prepare('SELECT key FROM data ORDER BY key').iterate();
    db.transaction(() => {
      t.assert.strictEqual(iterator.next().value.key, 1);
      db.exec('INSERT INTO data (key) VALUES (3)');
    });
    t.assert.strictEqual(db.isTransaction, false);
    t.assert.deepStrictEqual(keys(db), [1, 2, 3]);
  });

  test('allows sessions to be closed inside fn', (t) => {
    using db = createDb();
    const changeset = db.transaction(() => {
      const session = db.createSession();
      db.exec('INSERT INTO data (key) VALUES (1)');
      const changeset = session.changeset();
      session.close();
      return changeset;
    });
    t.assert.ok(changeset.length > 0);
    t.assert.deepStrictEqual(keys(db), [1]);
  });

  suite('authorizer', () => {
    test('throws without calling fn if BEGIN is denied', (t) => {
      using db = createDb();
      db.setAuthorizer((action) => {
        return action === constants.SQLITE_TRANSACTION ?
          constants.SQLITE_DENY : constants.SQLITE_OK;
      });
      const fn = t.mock.fn();
      t.assert.throws(() => {
        db.transaction(fn);
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /not authorized/,
      });
      t.assert.strictEqual(fn.mock.callCount(), 0);
      db.setAuthorizer(null);
      t.assert.strictEqual(db.isTransaction, false);
    });

    test('propagates an error thrown while authorizing BEGIN', (t) => {
      using db = createDb();
      const err = new Error('authorizer');
      db.setAuthorizer(() => { throw err; });
      const fn = t.mock.fn();
      t.assert.throws(() => {
        db.transaction(fn);
      }, (thrown) => thrown === err);
      t.assert.strictEqual(fn.mock.callCount(), 0);
    });

    test('rolls back if COMMIT is denied', (t) => {
      using db = createDb();
      db.setAuthorizer((action, operation) => {
        return action === constants.SQLITE_TRANSACTION &&
          operation === 'COMMIT' ? constants.SQLITE_DENY : constants.SQLITE_OK;
      });
      t.assert.throws(() => {
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (1)');
        });
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /not authorized/,
      });
      t.assert.strictEqual(db.isTransaction, false);
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('rolls back if the authorizer throws on COMMIT', (t) => {
      using db = createDb();
      const err = new Error('authorizer');
      db.setAuthorizer((action, operation) => {
        if (action === constants.SQLITE_TRANSACTION && operation === 'COMMIT') {
          throw err;
        }
        return constants.SQLITE_OK;
      });
      t.assert.throws(() => {
        db.transaction(() => {
          db.exec('INSERT INTO data (key) VALUES (1)');
        });
      }, (thrown) => thrown === err);
      t.assert.strictEqual(db.isTransaction, false);
      t.assert.deepStrictEqual(keys(db), []);
    });

    test('rethrows the error from fn if the rollback fails', (t) => {
      using db = createDb();
      const err = new Error('boom');
      db.setAuthorizer((action, operation) => {
        if (action === constants.SQLITE_TRANSACTION &&
            operation === 'ROLLBACK') {
          throw new Error('authorizer');
        }
        return constants.SQLITE_OK;
      });
      t.assert.throws(() => {
        db.transaction(() => {
          throw err;
        });
      }, (thrown) => thrown === err);

      // Later errors are still reported.
      db.setAuthorizer(null);
      t.assert.strictEqual(db.isTransaction, true);
      db.exec('ROLLBACK');
      t.assert.throws(() => {
        db.exec('SELECT * FROM missing');
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /no such table: missing/,
      });
    });

    test('nested transactions are authorized as savepoints', (t) => {
      using db = createDb();
      const savepoints = [];
      db.setAuthorizer((action, operation, name) => {
        if (action === constants.SQLITE_SAVEPOINT) {
          savepoints.push([operation, name]);
        }
        return constants.SQLITE_OK;
      });
      db.transaction(() => {
        db.transaction(() => {});
      });
      t.assert.deepStrictEqual(savepoints, [
        ['BEGIN', 'node_sqlite_transaction'],
        ['RELEASE', 'node_sqlite_transaction'],
      ]);
    });
  });
});
