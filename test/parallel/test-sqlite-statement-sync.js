// Flags: --expose-gc
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { DatabaseSync, StatementSync } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('StatementSync() constructor', () => {
  test('StatementSync cannot be constructed directly', (t) => {
    t.assert.throws(() => {
      new StatementSync();
    }, {
      code: 'ERR_ILLEGAL_CONSTRUCTOR',
      message: /Illegal constructor/,
    });
  });
});

suite('StatementSync.prototype.get()', () => {
  test('executes a query and returns undefined on no results', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.strictEqual(stmt.get(), undefined);
    stmt = db.prepare('SELECT * FROM storage');
    t.assert.strictEqual(stmt.get(), undefined);
  });

  test('executes a query and returns the first result', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.strictEqual(stmt.get(), undefined);
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.strictEqual(stmt.get('key1', 'val1'), undefined);
    t.assert.strictEqual(stmt.get('key2', 'val2'), undefined);
    stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, key: 'key1', val: 'val1' });
  });

  test('executes a query that returns special columns', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('SELECT 1 as __proto__, 2 as constructor, 3 as toString');
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, ['__proto__']: 1, constructor: 2, toString: 3 });
  });
});

suite('StatementSync.prototype.all()', () => {
  test('executes a query and returns an empty array on no results', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(stmt.all(), []);
  });

  test('executes a query and returns all results', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(stmt.run(), { changes: 0, lastInsertRowid: 0 });
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.deepStrictEqual(
      stmt.run('key1', 'val1'),
      { changes: 1, lastInsertRowid: 1 },
    );
    t.assert.deepStrictEqual(
      stmt.run('key2', 'val2'),
      { changes: 1, lastInsertRowid: 2 },
    );
    stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    t.assert.deepStrictEqual(stmt.all(), [
      { __proto__: null, key: 'key1', val: 'val1' },
      { __proto__: null, key: 'key2', val: 'val2' },
    ]);
  });
});

suite('StatementSync.prototype.iterate()', () => {
  test('executes a query and returns an empty iterator on no results', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    const iter = stmt.iterate();
    t.assert.strictEqual(iter instanceof globalThis.Iterator, true);
    t.assert.ok(iter[Symbol.iterator]);
    t.assert.deepStrictEqual(iter.toArray(), []);
  });

  test('executes a query and returns all results', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(stmt.run(), { changes: 0, lastInsertRowid: 0 });
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.deepStrictEqual(
      stmt.run('key1', 'val1'),
      { changes: 1, lastInsertRowid: 1 },
    );
    t.assert.deepStrictEqual(
      stmt.run('key2', 'val2'),
      { changes: 1, lastInsertRowid: 2 },
    );

    const items = [
      { __proto__: null, key: 'key1', val: 'val1' },
      { __proto__: null, key: 'key2', val: 'val2' },
    ];

    stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    t.assert.deepStrictEqual(stmt.iterate().toArray(), items);

    const itemsLoop = items.slice();
    for (const item of stmt.iterate()) {
      t.assert.deepStrictEqual(item, itemsLoop.shift());
    }
  });

  test('iterator keeps the prepared statement from being collected', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE test(key TEXT, val TEXT);
      INSERT INTO test (key, val) VALUES ('key1', 'val1');
      INSERT INTO test (key, val) VALUES ('key2', 'val2');
    `);
    // Do not keep an explicit reference to the prepared statement.
    const iterator = db.prepare('SELECT * FROM test').iterate();
    const results = [];

    global.gc();

    for (const item of iterator) {
      results.push(item);
    }

    t.assert.deepStrictEqual(results, [
      { __proto__: null, key: 'key1', val: 'val1' },
      { __proto__: null, key: 'key2', val: 'val2' },
    ]);
  });

  test('iterator can be exited early', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE test(key TEXT, val TEXT);
      INSERT INTO test (key, val) VALUES ('key1', 'val1');
      INSERT INTO test (key, val) VALUES ('key2', 'val2');
    `);
    const iterator = db.prepare('SELECT * FROM test').iterate();
    const results = [];

    for (const item of iterator) {
      results.push(item);
      break;
    }

    t.assert.deepStrictEqual(results, [
      { __proto__: null, key: 'key1', val: 'val1' },
    ]);
    t.assert.deepStrictEqual(
      iterator.next(),
      { __proto__: null, done: true, value: null },
    );
  });
});

suite('StatementSync.prototype.run()', () => {
  test('executes a query and returns change metadata', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE storage(key TEXT, val TEXT);
      INSERT INTO storage (key, val) VALUES ('foo', 'bar');
    `);
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('SELECT * FROM storage');
    t.assert.deepStrictEqual(stmt.run(), { changes: 1, lastInsertRowid: 1 });
  });

  test('SQLite throws when trying to bind too many parameters', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    t.assert.throws(() => {
      stmt.run(1, 2, 3);
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'column index out of range',
      errcode: 25,
      errstr: 'column index out of range',
    });
  });

  test('SQLite defaults to NULL for unbound parameters', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    t.assert.throws(() => {
      stmt.run(1);
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'NOT NULL constraint failed: data.val',
      errcode: 1299,
      errstr: 'constraint failed',
    });
  });

  test('returns correct metadata when using RETURNING', (t) => {
    const db = new DatabaseSync(':memory:');
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO data (key, val) VALUES ($k, $v) RETURNING key';
    const stmt = db.prepare(sql);
    t.assert.deepStrictEqual(
      stmt.run({ k: 1, v: 10 }), { changes: 1, lastInsertRowid: 1 }
    );
    t.assert.deepStrictEqual(
      stmt.run({ k: 2, v: 20 }), { changes: 1, lastInsertRowid: 2 }
    );
    t.assert.deepStrictEqual(
      stmt.run({ k: 3, v: 30 }), { changes: 1, lastInsertRowid: 3 }
    );
  });

  test('SQLite defaults unbound ?NNN parameters', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?1, ?3)');

    t.assert.throws(() => {
      stmt.run(1);
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'NOT NULL constraint failed: data.val',
      errcode: 1299,
      errstr: 'constraint failed',
    });
  });

  test('binds ?NNN params by position', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?1, ?2)');
    t.assert.deepStrictEqual(stmt.run(1, 2), { changes: 1, lastInsertRowid: 1 });
  });
});

suite('StatementSync.prototype.sourceSQL', () => {
  test('equals input SQL', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO types (key, val) VALUES ($k, $v)';
    const stmt = db.prepare(sql);
    t.assert.strictEqual(stmt.sourceSQL, sql);
  });
});

suite('StatementSync.prototype.expandedSQL', () => {
  test('equals expanded SQL', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO types (key, val) VALUES ($k, ?)';
    const expanded = 'INSERT INTO types (key, val) VALUES (\'33\', \'42\')';
    const stmt = db.prepare(sql);
    t.assert.deepStrictEqual(
      stmt.run({ $k: '33' }, '42'),
      { changes: 1, lastInsertRowid: 33 },
    );
    t.assert.strictEqual(stmt.expandedSQL, expanded);
  });
});

suite('StatementSync.prototype.get() with array output', () => {
  test('returns array row when returnArrays is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data WHERE key = 1', { returnArrays: true });
    t.assert.deepStrictEqual(query.get(), [1, 'one']);
  });

  test('returns array rows with BigInts when both flags are set', (t) => {
    const expected = [1n, 9007199254740992n];
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE big_data(id INTEGER, big_num INTEGER);
      INSERT INTO big_data VALUES (1, 9007199254740992);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT id, big_num FROM big_data', { returnArrays: true, readBigInts: true });

    const row = query.get();
    t.assert.deepStrictEqual(row, expected);
  });
});

suite('StatementSync.prototype.all() with array output', () => {
  test('returns array rows when returnArrays is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data ORDER BY key', { returnArrays: true });

    t.assert.deepStrictEqual(query.all(), [
      [1, 'one'],
      [2, 'two'],
    ]);
  });

  test('handles array rows with many columns', (t) => {
    const expected = [
      1,
      'text1',
      1.1,
      new Uint8Array([0xde, 0xad, 0xbe, 0xef]),
      5,
      'text2',
      2.2,
      new Uint8Array([0xbe, 0xef, 0xca, 0xfe]),
      9,
      'text3',
    ];
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE wide_table(
        col1 INTEGER, col2 TEXT, col3 REAL, col4 BLOB, col5 INTEGER,
        col6 TEXT, col7 REAL, col8 BLOB, col9 INTEGER, col10 TEXT
      );
      INSERT INTO wide_table VALUES (
        1, 'text1', 1.1, X'DEADBEEF', 5,
        'text2', 2.2, X'BEEFCAFE', 9, 'text3'
      );
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT * FROM wide_table', { returnArrays: true });

    const results = query.all();
    t.assert.strictEqual(results.length, 1);
    t.assert.deepStrictEqual(results[0], expected);
  });
});

suite('StatementSync.prototype.iterate() with array output', () => {
  test('iterates array rows when returnArrays is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data ORDER BY key', { returnArrays: true });

    const arrayRows = [];
    for (const row of query.iterate()) {
      arrayRows.push(row);
    }
    t.assert.deepStrictEqual(arrayRows, [
      [1, 'one'],
      [2, 'two'],
    ]);

    // Test toArray() method
    t.assert.deepStrictEqual(query.iterate().toArray(), [
      [1, 'one'],
      [2, 'two'],
    ]);
  });

  test('iterator can be exited early with array rows', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE test(key TEXT, val TEXT);
      INSERT INTO test (key, val) VALUES ('key1', 'val1');
      INSERT INTO test (key, val) VALUES ('key2', 'val2');
    `);
    const stmt = db.prepare('SELECT key, val FROM test', { returnArrays: true });

    const iterator = stmt.iterate();
    const results = [];

    for (const item of iterator) {
      results.push(item);
      break;
    }

    t.assert.deepStrictEqual(results, [
      ['key1', 'val1'],
    ]);
    t.assert.deepStrictEqual(
      iterator.next(),
      { __proto__: null, done: true, value: null },
    );
  });
});

suite('options.readBigInts', () => {
  test('BigInts are returned when input is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data', { readBigInts: true });
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42n });
  });

  test('numbers are returned when input is false', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data', { readBigInts: false });
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42 });
  });

  test('throws when input is not a boolean', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    t.assert.throws(() => {
      db.prepare('SELECT val FROM data', { readBigInts: 'true' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.readBigInts" argument must be a boolean/,
    });
  });
});

suite('options.returnArrays', () => {
  test('arrays are returned when input is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare(
      'SELECT key, val FROM data WHERE key = 1',
      { returnArrays: true }
    );
    t.assert.deepStrictEqual(query.get(), [1, 'one']);
  });

  test('objects are returned when input is false', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare(
      'SELECT key, val FROM data WHERE key = 1',
      { returnArrays: false }
    );
    t.assert.deepStrictEqual(query.get(), { __proto__: null, key: 1, val: 'one' });
  });

  test('throws when input is not a boolean', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    t.assert.throws(() => {
      db.prepare('SELECT key, val FROM data', { returnArrays: 'true' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.returnArrays" argument must be a boolean/,
    });
  });

  test('all() returns arrays when input is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare(
      'SELECT key, val FROM data ORDER BY key',
      { returnArrays: true }
    );
    t.assert.deepStrictEqual(query.all(), [
      [1, 'one'],
      [2, 'two'],
    ]);
  });

  test('iterate() returns arrays when input is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare(
      'SELECT key, val FROM data ORDER BY key',
      { returnArrays: true }
    );
    t.assert.deepStrictEqual(query.iterate().toArray(), [
      [1, 'one'],
      [2, 'two'],
    ]);
  });
});

suite('options.allowBareNamedParameters', () => {
  test('bare named parameters are allowed when input is true', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare(
      'INSERT INTO data (key, val) VALUES ($k, $v)',
      { allowBareNamedParameters: true }
    );
    t.assert.deepStrictEqual(
      stmt.run({ k: 1, v: 2 }),
      { changes: 1, lastInsertRowid: 1 },
    );
  });

  test('bare named parameters throw when input is false', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare(
      'INSERT INTO data (key, val) VALUES ($k, $v)',
      { allowBareNamedParameters: false }
    );
    t.assert.throws(() => {
      stmt.run({ k: 1, v: 2 });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Unknown named parameter 'k'/,
    });
  });

  test('throws when input is not a boolean', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    t.assert.throws(() => {
      db.prepare(
        'INSERT INTO data (key, val) VALUES ($k, $v)',
        { allowBareNamedParameters: 'true' }
      );
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.allowBareNamedParameters" argument must be a boolean/,
    });
  });
});
