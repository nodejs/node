import { skip, skipIfSQLiteMissing } from '../common/index.mjs';
skip();
import tmpdir from '../common/tmpdir.js';
import { existsSync } from 'node:fs';
import { suite, test } from 'node:test';
import { join } from 'node:path';
import { Database, Statement } from 'node:sqlite';
skipIfSQLiteMissing();

tmpdir.refresh();

let cnt = 0;
function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('Database() constructor', () => {
  test('throws if called without new', (t) => {
    t.assert.throws(() => {
      Database();
    }, {
      code: 'ERR_CONSTRUCT_CALL_REQUIRED',
      message: /Cannot call constructor without `new`/,
    });
  });

  test('throws if database path is not a string, Uint8Array, or URL', (t) => {
    t.assert.throws(() => {
      new Database();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "path" argument must be a string, Uint8Array, or URL without null bytes/,
    });
  });

  test('throws if the database location as Buffer contains null bytes', (t) => {
    t.assert.throws(() => {
      new Database(Buffer.from('l\0cation'));
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "path" argument must be a string, Uint8Array, or URL without null bytes.',
    });
  });

  test('throws if the database location as string contains null bytes', (t) => {
    t.assert.throws(() => {
      new Database('l\0cation');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "path" argument must be a string, Uint8Array, or URL without null bytes.',
    });
  });

  test('throws if options is provided but is not an object', (t) => {
    t.assert.throws(() => {
      new Database('foo', null);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options" argument must be an object/,
    });
  });

  test('throws if options.open is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { open: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.open" argument must be a boolean/,
    });
  });

  test('throws if options.readOnly is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { readOnly: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.readOnly" argument must be a boolean/,
    });
  });

  test('throws if options.timeout is provided but is not an integer', (t) => {
    t.assert.throws(() => {
      new Database('foo', { timeout: .99 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.timeout" argument must be an integer/,
    });
  });

  test('is not read-only by default', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath);
    await db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
  });

  test('is read-only if readOnly is set', async (t) => {
    const dbPath = nextDb();
    {
      const db = new Database(dbPath);
      await db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
      db.close();
    }
    {
      const db = new Database(dbPath, { readOnly: true });
      await t.assert.rejects(db.exec('CREATE TABLE bar (id INTEGER PRIMARY KEY)'), {
        code: 'ERR_SQLITE_ERROR',
        message: /attempt to write a readonly database/,
      });
    }
  });

  test('throws if options.enableForeignKeyConstraints is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { enableForeignKeyConstraints: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.enableForeignKeyConstraints" argument must be a boolean/,
    });
  });

  test('enables foreign key constraints by default', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath);
    await db.exec(`
      CREATE TABLE foo (id INTEGER PRIMARY KEY);
      CREATE TABLE bar (foo_id INTEGER REFERENCES foo(id));
    `);
    t.after(() => { db.close(); });
    await t.assert.rejects(
      db.exec('INSERT INTO bar (foo_id) VALUES (1)'),
      {
        code: 'ERR_SQLITE_ERROR',
        message: 'FOREIGN KEY constraint failed',
      });
  });

  test('allows disabling foreign key constraints', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { enableForeignKeyConstraints: false });
    await db.exec(`
      CREATE TABLE foo (id INTEGER PRIMARY KEY);
      CREATE TABLE bar (foo_id INTEGER REFERENCES foo(id));
    `);
    t.after(() => { db.close(); });
    await db.exec('INSERT INTO bar (foo_id) VALUES (1)');
  });

  test('throws if options.enableDoubleQuotedStringLiterals is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { enableDoubleQuotedStringLiterals: 5 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.enableDoubleQuotedStringLiterals" argument must be a boolean/,
    });
  });

  test('disables double-quoted string literals by default', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath);
    t.after(() => { db.close(); });
    await t.assert.rejects(db.exec('SELECT "foo";'), {
      code: 'ERR_SQLITE_ERROR',
      message: /no such column: "?foo"?/,
    });
  });

  test('allows enabling double-quoted string literals', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { enableDoubleQuotedStringLiterals: true });
    t.after(() => { db.close(); });
    await db.exec('SELECT "foo";');
  });

  test('throws if options.readBigInts is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { readBigInts: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.readBigInts" argument must be a boolean.',
    });
  });

  test('allows reading big integers', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { readBigInts: true });
    t.after(() => { db.close(); });

    const setup = await db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data');
    t.assert.deepStrictEqual(await query.get(), { __proto__: null, val: 42n });

    const insert = db.prepare('INSERT INTO data (key) VALUES (?)');
    t.assert.deepStrictEqual(
      await insert.run(20),
      { changes: 1n, lastInsertRowid: 20n },
    );
  });

  test('throws if options.returnArrays is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { returnArrays: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.returnArrays" argument must be a boolean.',
    });
  });

  test('allows returning arrays', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { returnArrays: true });
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data WHERE key = 1');
    t.assert.deepStrictEqual(await query.get(), [1, 'one']);
  });

  test('throws if options.allowBareNamedParameters is provided but is not a boolean', (t) => {
    t.assert.throws(() => {
      new Database('foo', { allowBareNamedParameters: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.allowBareNamedParameters" argument must be a boolean.',
    });
  });

  test('throws if bare named parameters are used when option is false', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { allowBareNamedParameters: false });
    t.after(() => { db.close(); });
    const setup = await db.exec(
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
      new Database('foo', { allowUnknownNamedParameters: 42 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.allowUnknownNamedParameters" argument must be a boolean.',
    });
  });

  test('allows unknown named parameters', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { allowUnknownNamedParameters: true });
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);

    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    const params = { $a: 1, $b: 2, $k: 42, $y: 25, $v: 84, $z: 99 };
    t.assert.deepStrictEqual(
      await stmt.run(params),
      { changes: 1, lastInsertRowid: 1 },
    );
  });

  test('has sqlite-type symbol property', (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath);
    t.after(() => { db.close(); });

    const sqliteTypeSymbol = Symbol.for('sqlite-type');
    t.assert.strictEqual(db[sqliteTypeSymbol], 'node:sqlite');
  });
});

suite('Database.prototype.open()', () => {
  test('opens a database connection', (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { open: false });
    t.after(() => { db.close(); });

    t.assert.strictEqual(db.isOpen, false);
    t.assert.strictEqual(existsSync(dbPath), false);
    t.assert.strictEqual(db.open(), undefined);
    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(existsSync(dbPath), true);
  });

  test('throws if database is already open', (t) => {
    const db = new Database(nextDb(), { open: false });
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

suite('Database.prototype.close()', () => {
  test('closes an open database connection', (t) => {
    const db = new Database(nextDb());

    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(db.close(), undefined);
    t.assert.strictEqual(db.isOpen, false);
  });

  test('throws if database is not open', (t) => {
    const db = new Database(nextDb(), { open: false });

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

suite('Database.prototype.prepare()', () => {
  test('returns a prepared statement', (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('CREATE TABLE webstorage(key TEXT)');
    t.assert.ok(stmt instanceof Statement);
  });

  test('throws if database is not open', (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.throws(() => {
      db.prepare();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if sql is not a string', (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.prepare();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});

suite('Database.prototype.exec()', () => {
  test('executes SQL', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const result = await db.exec(`
      CREATE TABLE data(
        key INTEGER PRIMARY KEY,
        val INTEGER
      ) STRICT;
      INSERT INTO data (key, val) VALUES (1, 2);
      INSERT INTO data (key, val) VALUES (8, 9);
    `);
    t.assert.strictEqual(result, undefined);
    const stmt = db.prepare('SELECT * FROM data ORDER BY key');
    t.assert.deepStrictEqual(await stmt.all(), [
      { __proto__: null, key: 1, val: 2 },
      { __proto__: null, key: 8, val: 9 },
    ]);
  });

  test('reports errors from SQLite', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });

    await t.assert.rejects(
      db.exec('CREATE TABLEEEE'),
      {
        code: 'ERR_SQLITE_ERROR',
        message: /syntax error/,
      });
  });

  test('throws if the URL does not have the file: scheme', (t) => {
    t.assert.throws(() => {
      new Database(new URL('http://example.com'));
    }, {
      code: 'ERR_INVALID_URL_SCHEME',
      message: 'The URL must be of scheme file:',
    });
  });

  test('throws if database is not open', (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if sql is not a string', (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});

suite('Database.prototype.isTransaction', () => {
  test('correctly detects a committed transaction', async (t) => {
    const db = new Database(':memory:');

    t.assert.strictEqual(db.isTransaction, false);
    await db.exec('BEGIN');
    t.assert.strictEqual(db.isTransaction, true);
    await db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
    t.assert.strictEqual(db.isTransaction, true);
    await db.exec('COMMIT');
    t.assert.strictEqual(db.isTransaction, false);
  });

  test('correctly detects a rolled back transaction', async (t) => {
    const db = new Database(':memory:');

    t.assert.strictEqual(db.isTransaction, false);
    await db.exec('BEGIN');
    t.assert.strictEqual(db.isTransaction, true);
    await db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
    t.assert.strictEqual(db.isTransaction, true);
    await db.exec('ROLLBACK');
    t.assert.strictEqual(db.isTransaction, false);
  });

  test('throws if database is not open', (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.throws(() => {
      return db.isTransaction;
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });
});

suite('Database.prototype.location()', () => {
  test('throws if database is not open', (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.throws(() => {
      db.location();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if provided dbName is not string', (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.location(null);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "dbName" argument must be a string/,
    });
  });

  test('returns null when connected to in-memory database', (t) => {
    const db = new Database(':memory:');
    t.assert.strictEqual(db.location(), null);
  });

  test('returns db path when connected to a persistent database', (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath);
    t.after(() => { db.close(); });
    t.assert.strictEqual(db.location(), dbPath);
  });

  test('returns that specific db path when attached', async (t) => {
    const dbPath = nextDb();
    const otherPath = nextDb();
    const db = new Database(dbPath);
    t.after(() => { db.close(); });
    const other = new Database(dbPath);
    t.after(() => { other.close(); });

    // Adding this escape because the test with unusual chars have a single quote which breaks the query
    const escapedPath = otherPath.replace("'", "''");
    await db.exec(`ATTACH DATABASE '${escapedPath}' AS other`);

    t.assert.strictEqual(db.location('other'), otherPath);
  });
});

suite('Async mode restrictions', () => {
  test('throws when defining a custom function', (t) => {
    const db = new Database(':memory:');
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.function('test', () => 1);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Custom functions are not supported in async mode/,
    });
  });

  test('throws when defining an aggregate function', (t) => {
    const db = new Database(':memory:');
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.aggregate('test', { start: 0, step: (acc) => acc });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Aggregate functions are not supported in async mode/,
    });
  });

  test('throws when setting an authorizer callback', (t) => {
    const db = new Database(':memory:');
    t.after(() => { db.close(); });

    t.assert.throws(() => {
      db.setAuthorizer(() => 0);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Authorizer callbacks are not supported in async mode/,
    });
  });
});

suite('Async operation ordering', () => {
  test('executes operations sequentially per database', async (t) => {
    const db = new Database(':memory:');
    t.after(() => { db.close(); });
    await db.exec('CREATE TABLE test (id INTEGER PRIMARY KEY, seq INTEGER)');

    // Launch multiple operations concurrently
    const ops = [];
    for (let i = 0; i < 10; i++) {
      ops.push(db.exec(`INSERT INTO test (seq) VALUES (${i})`));
    }

    await Promise.all(ops);

    // Check they were inserted in order (sequential execution)
    const stmt = db.prepare('SELECT id, seq FROM test ORDER BY id');
    const rows = await stmt.all();

    // Verify sequential: id should match seq + 1 (autoincrement starts at 1)
    t.assert.strictEqual(rows.length, 10);
    for (const row of rows) {
      t.assert.strictEqual(row.id, row.seq + 1);
    }
  });

  test('different connections can execute in parallel', async (t) => {
    const db1 = new Database(':memory:');
    const db2 = new Database(':memory:');
    t.after(() => { db1.close(); db2.close(); });
    const times = {};
    const now = () => process.hrtime.bigint();
    const LONG_QUERY = `
      WITH RECURSIVE cnt(x) AS (
        SELECT 1
        UNION ALL
        SELECT x + 1 FROM cnt WHERE x < 300000
      )
      SELECT sum(x) FROM cnt;
  `;

    const op = async (db, label) => {
      times[label] = { start: now() };

      await db.exec(LONG_QUERY);

      times[label].end = now();
    };

    // Start both operations
    await Promise.all([op(db1, 'db1'), op(db2, 'db2')]);

    // Verify that their execution times overlap
    t.assert.ok(
      times.db1.start < times.db2.end &&
      times.db2.start < times.db1.end
    );
  });
});

suite('Database.prototype.close', () => {
  test('rejects pending operations when database is closed', async (t) => {
    const db = new Database(':memory:');
    await db.exec('CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)');
    const pendingOp = db.exec('INSERT INTO test (value) VALUES (\'test\')');
    db.close();

    await t.assert.rejects(pendingOp, {
      code: 'ERR_INVALID_STATE',
      message: /database is closing/,
    });
  });

  test('rejects multiple pending operations when database is closed', async (t) => {
    const db = new Database(':memory:');
    await db.exec('CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)');
    const ops = [
      db.exec('INSERT INTO test (value) VALUES (\'test1\')'),
      db.exec('INSERT INTO test (value) VALUES (\'test2\')'),
      db.exec('INSERT INTO test (value) VALUES (\'test3\')'),
    ];

    db.close();

    const results = await Promise.allSettled(ops);
    for (const result of results) {
      t.assert.strictEqual(result.status, 'rejected');
      t.assert.strictEqual(result.reason.code, 'ERR_INVALID_STATE');
    }
  });
});
