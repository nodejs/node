'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { existsSync } = require('node:fs');
const { join } = require('node:path');
const { DatabaseSync, StatementSync } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('DatabaseSync() constructor', () => {
  test('throws if called without new', (t) => {
    t.assert.throws(() => {
      DatabaseSync();
    }, {
      code: 'ERR_CONSTRUCT_CALL_REQUIRED',
      message: /Cannot call constructor without `new`/,
    });
  });

  test('throws if database path is not a string, Uint8Array, or URL', (t) => {
    t.assert.throws(() => {
      new DatabaseSync();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "path" argument must be a string, Uint8Array, or URL without null bytes/,
    });
  });

  test('throws if the database location as Buffer contains null bytes', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(Buffer.from('l\0cation'));
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "path" argument must be a string, Uint8Array, or URL without null bytes.',
    });
  });

  test('throws if the database location as string contains null bytes', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('l\0cation');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "path" argument must be a string, Uint8Array, or URL without null bytes.',
    });
  });

  test('throws if options is provided but is not an object', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', null);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options" argument must be an object/,
    });
  });

  test('throws if options.open is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { open: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.open" argument must be a boolean/,
    });
  });

  test('throws if options.readOnly is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { readOnly: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.readOnly" argument must be a boolean/,
    });
  });

  test('throws if options.timeout is provided but is not an integer', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { timeout: .99 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.timeout" argument must be an integer/,
    });
  });

  test('is not read-only by default', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath);
    db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
  });

  test('is read-only if readOnly is set', (t) => {
    const dbPath = nextDb();
    {
      const db = new DatabaseSync(dbPath);
      db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
      db.close();
    }
    {
      const db = new DatabaseSync(dbPath, { readOnly: true });
      t.assert.throws(() => {
        db.exec('CREATE TABLE bar (id INTEGER PRIMARY KEY)');
      }, {
        code: 'ERR_SQLITE_ERROR',
        message: /attempt to write a readonly database/,
      });
    }
  });

  test('throws if options.enableForeignKeyConstraints is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { enableForeignKeyConstraints: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.enableForeignKeyConstraints" argument must be a boolean/,
    });
  });

  test('enables foreign key constraints by default', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath);
    db.exec(`
      CREATE TABLE foo (id INTEGER PRIMARY KEY);
      CREATE TABLE bar (foo_id INTEGER REFERENCES foo(id));
    `);
    t.after(() => { db.close(); });
    t.assert.throws(() => {
      db.exec('INSERT INTO bar (foo_id) VALUES (1)');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'FOREIGN KEY constraint failed',
    });
  });

  test('allows disabling foreign key constraints', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { enableForeignKeyConstraints: false });
    db.exec(`
      CREATE TABLE foo (id INTEGER PRIMARY KEY);
      CREATE TABLE bar (foo_id INTEGER REFERENCES foo(id));
    `);
    t.after(() => { db.close(); });
    db.exec('INSERT INTO bar (foo_id) VALUES (1)');
  });

  test('throws if options.enableDoubleQuotedStringLiterals is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { enableDoubleQuotedStringLiterals: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.enableDoubleQuotedStringLiterals" argument must be a boolean/,
    });
  });

  test('disables double-quoted string literals by default', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath);
    t.after(() => { db.close(); });
    t.assert.throws(() => {
      db.exec('SELECT "foo";');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /no such column: "?foo"?/,
    });
  });

  test('allows enabling double-quoted string literals', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { enableDoubleQuotedStringLiterals: true });
    t.after(() => { db.close(); });
    db.exec('SELECT "foo";');
  });

  test('throws if options.readBigInts is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { readBigInts: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.readBigInts" argument must be a boolean.',
    });
  });

  test('allows reading big integers', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { readBigInts: true });
    t.after(() => { db.close(); });

    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data');
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42n });

    const insert = db.prepare('INSERT INTO data (key) VALUES (?)');
    t.assert.deepStrictEqual(
      insert.run(20),
      { changes: 1n, lastInsertRowid: 20n },
    );
  });

  test('throws if options.readNullAsUndefined is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { readNullAsUndefined: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.readNullAsUndefined" argument must be a boolean.',
    });
  });

  test('SQL null can be read as undefined', (t) => {
    const db = new DatabaseSync(nextDb(), { readNullAsUndefined: true });
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(is_null TEXT DEFAULT NULL) STRICT;
      INSERT INTO data(is_null) VALUES(NULL);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT is_null FROM DATA');
    t.assert.deepStrictEqual(query.get(), { __proto__: null, is_null: null });
    t.assert.strictEqual(query.setReadNullAsUndefined(true), undefined);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, is_null: undefined });
    t.assert.strictEqual(query.setReadNullAsUndefined(false), undefined);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, is_null: null });
  });

  test('throws if options.returnArrays is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { returnArrays: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.returnArrays" argument must be a boolean.',
    });
  });

  test('allows returning arrays', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { returnArrays: true });
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data WHERE key = 1');
    t.assert.deepStrictEqual(query.get(), [1, 'one']);
  });

  test('throws if options.allowBareNamedParameters is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { allowBareNamedParameters: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.allowBareNamedParameters" argument must be a boolean.',
    });
  });

  test('throws if bare named parameters are used when option is false', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { allowBareNamedParameters: false });
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);

    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    t.assert.throws(() => {
      stmt.run({ k: 2, v: 4 });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Unknown named parameter 'k'/,
    });
  });

  test('throws if options.allowUnknownNamedParameters is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new DatabaseSync('foo', { allowUnknownNamedParameters: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.allowUnknownNamedParameters" argument must be a boolean.',
    });
  });

  test('allows unknown named parameters', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { allowUnknownNamedParameters: true });
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);

    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    const params = { $a: 1, $b: 2, $k: 42, $y: 25, $v: 84, $z: 99 };
    t.assert.deepStrictEqual(
      stmt.run(params),
      { changes: 1, lastInsertRowid: 1 },
    );
  });
});

suite('DatabaseSync.prototype.open()', () => {
  test('opens a database connection', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath, { open: false });
    t.after(() => { db.close(); });

    t.assert.strictEqual(db.isOpen, false);
    t.assert.strictEqual(existsSync(dbPath), false);
    t.assert.strictEqual(db.open(), undefined);
    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(existsSync(dbPath), true);
  });

  test('throws if database is already open', (t) => {
    const db = new DatabaseSync(nextDb(), { open: false });
    t.after(() => { db.close(); });

    t.assert.strictEqual(db.isOpen, false);
    db.open();
    t.assert.strictEqual(db.isOpen, true);
    t.assert.throws(() => {
      db.open();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is already open/,
    });
    t.assert.strictEqual(db.isOpen, true);
  });
});

suite('DatabaseSync.prototype.close()', () => {
  test('closes an open database connection', (t) => {
    const db = new DatabaseSync(nextDb());

    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(db.close(), undefined);
    t.assert.strictEqual(db.isOpen, false);
  });

  test('throws if database is not open', (t) => {
    const db = new DatabaseSync(nextDb(), { open: false });

    t.assert.strictEqual(db.isOpen, false);
    t.assert.throws(() => {
      db.close();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
    t.assert.strictEqual(db.isOpen, false);
  });
});

suite('DatabaseSync.prototype.prepare()', () => {
  test('returns a prepared statement', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('CREATE TABLE webstorage(key TEXT)');
    t.assert.ok(stmt instanceof StatementSync);
  });

  test('throws if database is not open', (t) => {
    const db = new DatabaseSync(nextDb(), { open: false });

    t.assert.throws(() => {
      db.prepare();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if sql is not a string', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.prepare();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});

suite('DatabaseSync.prototype.exec()', () => {
  test('executes SQL', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const result = db.exec(`
      CREATE TABLE data(
        key INTEGER PRIMARY KEY,
        val INTEGER
      ) STRICT;
      INSERT INTO data (key, val) VALUES (1, 2);
      INSERT INTO data (key, val) VALUES (8, 9);
    `);
    t.assert.strictEqual(result, undefined);
    const stmt = db.prepare('SELECT * FROM data ORDER BY key');
    t.assert.deepStrictEqual(stmt.all(), [
      { __proto__: null, key: 1, val: 2 },
      { __proto__: null, key: 8, val: 9 },
    ]);
  });

  test('reports errors from SQLite', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.exec('CREATE TABLEEEE');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /syntax error/,
    });
  });

  test('throws if the URL does not have the file: scheme', (t) => {
    t.assert.throws(() => {
      new DatabaseSync(new URL('http://example.com'));
    }, {
      code: 'ERR_INVALID_URL_SCHEME',
      message: 'The URL must be of scheme file:',
    });
  });

  test('throws if database is not open', (t) => {
    const db = new DatabaseSync(nextDb(), { open: false });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if sql is not a string', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});

suite('DatabaseSync.prototype.isTransaction', () => {
  test('correctly detects a committed transaction', (t) => {
    const db = new DatabaseSync(':memory:');

    t.assert.strictEqual(db.isTransaction, false);
    db.exec('BEGIN');
    t.assert.strictEqual(db.isTransaction, true);
    db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
    t.assert.strictEqual(db.isTransaction, true);
    db.exec('COMMIT');
    t.assert.strictEqual(db.isTransaction, false);
  });

  test('correctly detects a rolled back transaction', (t) => {
    const db = new DatabaseSync(':memory:');

    t.assert.strictEqual(db.isTransaction, false);
    db.exec('BEGIN');
    t.assert.strictEqual(db.isTransaction, true);
    db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
    t.assert.strictEqual(db.isTransaction, true);
    db.exec('ROLLBACK');
    t.assert.strictEqual(db.isTransaction, false);
  });

  test('throws if database is not open', (t) => {
    const db = new DatabaseSync(nextDb(), { open: false });

    t.assert.throws(() => {
      return db.isTransaction;
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });
});

suite('DatabaseSync.prototype.location()', () => {
  test('throws if database is not open', (t) => {
    const db = new DatabaseSync(nextDb(), { open: false });

    t.assert.throws(() => {
      db.location();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if provided dbName is not string', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.location(null);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "dbName" argument must be a string/,
    });
  });

  test('returns null when connected to in-memory database', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.strictEqual(db.location(), null);
  });

  test('returns db path when connected to a persistent database', (t) => {
    const dbPath = nextDb();
    const db = new DatabaseSync(dbPath);
    t.after(() => { db.close(); });
    t.assert.strictEqual(db.location(), dbPath);
  });

  test('returns that specific db path when attached', (t) => {
    const dbPath = nextDb();
    const otherPath = nextDb();
    const db = new DatabaseSync(dbPath);
    t.after(() => { db.close(); });
    const other = new DatabaseSync(dbPath);
    t.after(() => { other.close(); });

    // Adding this escape because the test with unusual chars have a single quote which breaks the query
    const escapedPath = otherPath.replace("'", "''");
    db.exec(`ATTACH DATABASE '${escapedPath}' AS other`);

    t.assert.strictEqual(db.location('other'), otherPath);
  });
});
