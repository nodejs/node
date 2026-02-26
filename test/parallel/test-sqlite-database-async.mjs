import { skipIfSQLiteMissing } from '../common/index.mjs';
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

suite('Database() constructor', { timeout: 1000 }, () => {
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

  test('throws if the database URL does not have the file: scheme', (t) => {
    t.assert.throws(() => {
      new Database(new URL('http://example.com'));
    }, {
      code: 'ERR_INVALID_URL_SCHEME',
      message: 'The URL must be of scheme file:',
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
    t.after(async () => { await db[Symbol.asyncDispose](); });
    await db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
  });

  test('is read-only if readOnly is set', async (t) => {
    const dbPath = nextDb();
    {
      // TODO: use `await using` once it's supported in test files
      const db = new Database(dbPath);
      try {
        await db.exec('CREATE TABLE foo (id INTEGER PRIMARY KEY)');
      } finally {
        await db.close();
      }
    }
    {
      const db = new Database(dbPath, { readOnly: true });
      t.after(async () => { await db[Symbol.asyncDispose](); });
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
    t.after(async () => { await db[Symbol.asyncDispose](); });
    await db.exec(`
      CREATE TABLE foo (id INTEGER PRIMARY KEY);
      CREATE TABLE bar (foo_id INTEGER REFERENCES foo(id));
    `);
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
    t.after(async () => { await db.close(); });
    await db.exec(`
      CREATE TABLE foo (id INTEGER PRIMARY KEY);
      CREATE TABLE bar (foo_id INTEGER REFERENCES foo(id));
    `);
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
    t.after(async () => { await db.close(); });
    await t.assert.rejects(db.exec('SELECT "foo";'), {
      code: 'ERR_SQLITE_ERROR',
      message: /no such column: "?foo"?/,
    });
  });

  test('allows enabling double-quoted string literals', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { enableDoubleQuotedStringLiterals: true });
    t.after(async () => { await db.close(); });
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

  test.skip('allows reading big integers', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { readBigInts: true });
    t.after(async () => { await db.close(); });

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

  test.skip('allows returning arrays', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { returnArrays: true });
    t.after(async () => { await db.close(); });
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

  test.skip('throws if bare named parameters are used when option is false', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { allowBareNamedParameters: false });
    t.after(async () => { await db.close(); });
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

  test.skip('allows unknown named parameters', async (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { allowUnknownNamedParameters: true });
    t.after(async () => { await db.close(); });
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
    const db = new Database(dbPath, { open: false });

    const sqliteTypeSymbol = Symbol.for('sqlite-type');
    t.assert.strictEqual(db[sqliteTypeSymbol], 'node:sqlite-async');
  });
});

suite('Database.prototype.open()', { timeout: 1000 }, () => {
  test('opens a database connection', (t) => {
    const dbPath = nextDb();
    const db = new Database(dbPath, { open: false });
    t.after(async () => { await db.close(); });

    t.assert.strictEqual(db.isOpen, false);
    t.assert.strictEqual(existsSync(dbPath), false);
    t.assert.strictEqual(db.open(), undefined);
    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(existsSync(dbPath), true);
  });

  test('throws if database is already open', (t) => {
    const db = new Database(nextDb(), { open: false });
    t.after(async () => { await db.close(); });

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

suite('Database.prototype.close()', { timeout: 1000 }, () => {
  test('closes an open database connection', async (t) => {
    const db = new Database(nextDb());

    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(await db.close(), undefined);
    t.assert.strictEqual(db.isOpen, false);
  });

  test('throws if database is not open', async (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.strictEqual(db.isOpen, false);
    await t.assert.rejects(db.close(), {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
    t.assert.strictEqual(db.isOpen, false);
  });

  // These tests need to be rewritten to account for the fact that close() is
  // async, so instead of rejecting pending operations, it will wait for them to
  // finish before closing the database. However, we can still test that new
  // operations are rejected once the database is closing.
  test.skip('rejects pending operations when database is closed', async (t) => {
    const db = new Database(':memory:');
    await db.exec('CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)');
    const pendingOp = db.exec('INSERT INTO test (value) VALUES (\'test\')');
    await db.close();

    await t.assert.rejects(pendingOp, {
      code: 'ERR_INVALID_STATE',
      message: /database is closing/,
    });
  });

  test.skip('rejects multiple pending operations when database is closed', async (t) => {
    const db = new Database(':memory:');
    await db.exec('CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)');
    const ops = [
      db.exec('INSERT INTO test (value) VALUES (\'test1\')'),
      db.exec('INSERT INTO test (value) VALUES (\'test2\')'),
      db.exec('INSERT INTO test (value) VALUES (\'test3\')'),
    ];

    await db.close();

    const results = await Promise.allSettled(ops);
    t.assert.partialDeepStrictEqual(
      results,
      ops.map(() => ({
        status: 'rejected',
        reason: {
          code: 'ERR_INVALID_STATE',
          message: /database is closing/,
        },
      }))
    );
    t.assert.strictEqual(results.length, ops.length);
  });
});

suite('Database.prototype[Symbol.asyncDispose]()', { timeout: 1000 }, () => {
  test('closes an open database connection', async (t) => {
    const db = new Database(nextDb());

    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(await db[Symbol.asyncDispose](), undefined);
    t.assert.strictEqual(db.isOpen, false);
  });
  test('does not throw if the database is not open', async (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.strictEqual(db.isOpen, false);
    t.assert.strictEqual(await db[Symbol.asyncDispose](), undefined);
    t.assert.strictEqual(db.isOpen, false);
  });
  test('prevents a database from being opened after disposal', async (t) => {
    const db = new Database(nextDb(), { open: false });

    t.assert.strictEqual(db.isOpen, false);
    t.assert.strictEqual(await db[Symbol.asyncDispose](), undefined);
    t.assert.strictEqual(db.isOpen, false);
    t.assert.throws(() => {
      db.open();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is disposed/,
    });
    t.assert.strictEqual(db.isOpen, false);
  });
  test('is idempotent', async (t) => {
    const db = new Database(nextDb());

    t.assert.strictEqual(db.isOpen, true);
    t.assert.strictEqual(await db[Symbol.asyncDispose](), undefined);
    t.assert.strictEqual(db.isOpen, false);
    t.assert.strictEqual(await db[Symbol.asyncDispose](), undefined);
    t.assert.strictEqual(db.isOpen, false);
  });
});

suite.skip('Database.prototype.prepare()', { timeout: 1000 }, () => {
  test('returns a prepared statement', (t) => {
    const db = new Database(nextDb());
    t.after(async () => { await db.close(); });
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
    t.after(async () => { await db.close(); });

    t.assert.throws(() => {
      db.prepare();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});

suite('Database.prototype.exec()', { timeout: 1000 }, () => {
  test.skip('executes SQL', async (t) => {
    const db = new Database(nextDb());
    t.after(async () => { await db.close(); });
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
    t.after(async () => { await db.close(); });

    await t.assert.rejects(
      db.exec('CREATE TABLEEEE'),
      {
        code: 'ERR_SQLITE_ERROR',
        message: /syntax error/,
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
    t.after(async () => { await db.close(); });

    t.assert.throws(() => {
      db.exec();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "sql" argument must be a string/,
    });
  });
});

suite.skip('Database.prototype.isTransaction', { timeout: 1000 }, () => {
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

suite.skip('Database.prototype.location()', { timeout: 1000 }, () => {
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
    t.after(async () => { await db.close(); });

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
    t.after(async () => { await db.close(); });
    t.assert.strictEqual(db.location(), dbPath);
  });

  test('returns that specific db path when attached', async (t) => {
    const dbPath = nextDb();
    const otherPath = nextDb();
    const db = new Database(dbPath);
    t.after(async () => { await db.close(); });
    const other = new Database(dbPath);
    t.after(async () => { await other.close(); });

    // Adding this escape because the test with unusual chars have a single quote which breaks the query
    const escapedPath = otherPath.replace("'", "''");
    await db.exec(`ATTACH DATABASE '${escapedPath}' AS other`);

    t.assert.strictEqual(db.location('other'), otherPath);
  });
});

suite('Async operation ordering', { timeout: 1000 }, () => {
  test.skip('executes operations sequentially per database', async (t) => {
    const db = new Database(':memory:');
    t.after(async () => { await db.close(); });
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

  test('different connections can execute in parallel', { timeout: 5000 }, async (t) => {
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
