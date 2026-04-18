'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

suite('DatabaseSync.prototype.serialize()', () => {
  test('returns a Uint8Array with the SQLite header', (t) => {
    const db = new DatabaseSync(':memory:');
    const buf = db.serialize();
    t.assert.ok(buf instanceof Uint8Array);
    t.assert.ok(buf.length > 0);
    const header = new TextDecoder().decode(buf.slice(0, 15));
    t.assert.strictEqual(header, 'SQLite format 3');
    db.close();
  });

  test('serializes an empty database', (t) => {
    const db = new DatabaseSync(':memory:');
    const buf = db.serialize();
    t.assert.ok(buf instanceof Uint8Array);
    t.assert.ok(buf.length > 0);
    db.close();
  });

  test('serializes a database with data', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT)');
    db.exec("INSERT INTO t VALUES (1, 'hello')");
    db.exec("INSERT INTO t VALUES (2, 'world')");
    const buf = db.serialize();
    t.assert.ok(buf.length > 0);
    db.close();
  });

  test('throws if the database is not open', (t) => {
    const db = new DatabaseSync(':memory:');
    db.close();
    t.assert.throws(() => {
      db.serialize();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if dbName is not a string', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.serialize(123);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "dbName" argument must be a string/,
    });
    db.close();
  });

  test('accepts a schema name argument', (t) => {
    const db = new DatabaseSync(':memory:');
    const buf = db.serialize('main');
    t.assert.ok(buf instanceof Uint8Array);
    t.assert.ok(buf.length > 0);
    db.close();
  });

  test('serializes an attached schema when dbName is provided', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec("ATTACH DATABASE ':memory:' AS aux");
    db.exec('CREATE TABLE aux.t(value TEXT)');
    db.exec("INSERT INTO aux.t VALUES ('from aux')");

    const buf = db.serialize('aux');
    db.close();

    const clone = new DatabaseSync(':memory:');
    clone.deserialize(buf);

    const row = clone.prepare('SELECT value FROM t').get();
    t.assert.strictEqual(row.value, 'from aux');
    clone.close();
  });
});

suite('DatabaseSync.prototype.deserialize()', () => {
  test('loads a serialized database', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec('CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT)');
    db1.exec("INSERT INTO t VALUES (1, 'hello')");
    db1.exec("INSERT INTO t VALUES (2, 'world')");
    const buf = db1.serialize();
    db1.close();

    const db2 = new DatabaseSync(':memory:');
    db2.deserialize(buf);
    const rows = db2.prepare('SELECT * FROM t ORDER BY id').all();
    t.assert.strictEqual(rows.length, 2);
    t.assert.strictEqual(rows[0].name, 'hello');
    t.assert.strictEqual(rows[1].name, 'world');
    db2.close();
  });

  test('replaces existing data in the connection', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec('CREATE TABLE src(val TEXT)');
    db1.exec("INSERT INTO src VALUES ('from source')");
    const buf = db1.serialize();
    db1.close();

    const db2 = new DatabaseSync(':memory:');
    db2.exec('CREATE TABLE old(x INTEGER)');
    db2.exec('INSERT INTO old VALUES (999)');
    db2.deserialize(buf);

    t.assert.throws(() => {
      db2.prepare('SELECT * FROM old').all();
    }, /no such table: old/);

    const rows = db2.prepare('SELECT * FROM src').all();
    t.assert.strictEqual(rows.length, 1);
    t.assert.strictEqual(rows[0].val, 'from source');
    db2.close();
  });

  test('finalizes existing prepared statements before replacing the database',
       (t) => {
         const db1 = new DatabaseSync(':memory:');
         db1.exec('CREATE TABLE replacement(value TEXT)');
         db1.exec("INSERT INTO replacement VALUES ('new')");
         const buf = db1.serialize();
         db1.close();

         const db2 = new DatabaseSync(':memory:');
         db2.exec('CREATE TABLE original(value TEXT)');
         db2.exec("INSERT INTO original VALUES ('old')");
         const stmt = db2.prepare('SELECT value FROM original');

         t.assert.strictEqual(stmt.get().value, 'old');

         db2.deserialize(buf);

         t.assert.throws(() => {
           stmt.get();
         }, /statement has been finalized/);

         const row = db2.prepare('SELECT value FROM replacement').get();
         t.assert.strictEqual(row.value, 'new');
         db2.close();
       });

  test('deserialized database is writable by default', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec('CREATE TABLE t(id INTEGER PRIMARY KEY)');
    const buf = db1.serialize();
    db1.close();

    const db2 = new DatabaseSync(':memory:');
    db2.deserialize(buf);
    db2.exec('INSERT INTO t VALUES (1)');
    const rows = db2.prepare('SELECT * FROM t').all();
    t.assert.strictEqual(rows.length, 1);
    db2.close();
  });

  test('round-trip serialize then deserialize preserves data', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec('CREATE TABLE t(a TEXT, b REAL, c BLOB)');
    db1.prepare('INSERT INTO t VALUES (?, ?, ?)').run(
      'text', 3.14, new Uint8Array([1, 2, 3])
    );
    const buf = db1.serialize();

    const db2 = new DatabaseSync(':memory:');
    db2.deserialize(buf);
    const row = db2.prepare('SELECT * FROM t').get();
    t.assert.strictEqual(row.a, 'text');
    t.assert.strictEqual(row.b, 3.14);
    t.assert.deepStrictEqual(row.c, new Uint8Array([1, 2, 3]));
    db1.close();
    db2.close();
  });

  test('throws if the database is not open', (t) => {
    const db = new DatabaseSync(':memory:');
    db.close();
    t.assert.throws(() => {
      db.deserialize(new Uint8Array(0));
    }, {
      code: 'ERR_INVALID_STATE',
      message: /database is not open/,
    });
  });

  test('throws if buffer argument is not a Uint8Array', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize('not a buffer');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "buffer" argument must be a Uint8Array/,
    });
    db.close();
  });

  test('throws if buffer is empty', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize(new Uint8Array(0));
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /The "buffer" argument must not be empty/,
    });
    db.close();
  });

  test('throws if options is not an object', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize(new Uint8Array(1), 'bad');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options" argument must be an object/,
    });
    db.close();
  });

  test('throws if options.dbName is not a string', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.deserialize(new Uint8Array(1), { dbName: 1 });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.dbName" argument must be a string/,
    });
    db.close();
  });

  test('accepts a Buffer as input', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec('CREATE TABLE t(x INTEGER)');
    db1.exec('INSERT INTO t VALUES (42)');
    const buf = Buffer.from(db1.serialize());
    db1.close();

    const db2 = new DatabaseSync(':memory:');
    db2.deserialize(buf);
    const row = db2.prepare('SELECT * FROM t').get();
    t.assert.strictEqual(row.x, 42);
    db2.close();
  });

  test('multiple deserialize calls on the same connection', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec('CREATE TABLE a(x)');
    db1.exec("INSERT INTO a VALUES ('first')");
    const buf1 = db1.serialize();
    db1.close();

    const db2 = new DatabaseSync(':memory:');
    db2.exec('CREATE TABLE b(x)');
    db2.exec("INSERT INTO b VALUES ('second')");
    const buf2 = db2.serialize();
    db2.close();

    const db3 = new DatabaseSync(':memory:');
    db3.deserialize(buf1);
    t.assert.strictEqual(
      db3.prepare('SELECT x FROM a').get().x, 'first'
    );

    db3.deserialize(buf2);
    t.assert.throws(() => {
      db3.prepare('SELECT * FROM a').all();
    }, /no such table: a/);
    t.assert.strictEqual(
      db3.prepare('SELECT x FROM b').get().x, 'second'
    );
    db3.close();
  });

  test('loads into an attached schema when options.dbName is provided', (t) => {
    const db1 = new DatabaseSync(':memory:');
    db1.exec("ATTACH DATABASE ':memory:' AS aux");
    db1.exec('CREATE TABLE aux.t(value TEXT)');
    db1.exec("INSERT INTO aux.t VALUES ('from aux')");
    const buf = db1.serialize('aux');
    db1.close();

    const db2 = new DatabaseSync(':memory:');
    db2.exec('CREATE TABLE main_t(value TEXT)');
    db2.exec("INSERT INTO main_t VALUES ('from main')");
    db2.exec("ATTACH DATABASE ':memory:' AS aux");

    db2.deserialize(buf, { dbName: 'aux' });

    t.assert.strictEqual(
      db2.prepare('SELECT value FROM main_t').get().value,
      'from main',
    );
    t.assert.strictEqual(
      db2.prepare('SELECT value FROM aux.t').get().value,
      'from aux',
    );
    db2.close();
  });
});
