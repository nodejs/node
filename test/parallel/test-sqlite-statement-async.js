// Flags: --expose-gc
'use strict';
const { skip, skipIfSQLiteMissing } = require('../common');
skip(); // TODO: Re-enable when async Statement support is added.
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { Database, Statement } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('Statement() constructor', () => {
  test('Statement cannot be constructed directly', (t) => {
    t.assert.throws(() => {
      new Statement();
    }, {
      code: 'ERR_ILLEGAL_CONSTRUCTOR',
      message: /Illegal constructor/,
    });
  });
});

suite('Statement.prototype.get()', () => {
  test('executes a query and returns undefined on no results', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.strictEqual(await stmt.get(), undefined);
    stmt = db.prepare('SELECT * FROM storage');
    t.assert.strictEqual(await stmt.get(), undefined);
  });

  test('executes a query and returns the first result', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.strictEqual(await stmt.get(), undefined);
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.strictEqual(await stmt.get('key1', 'val1'), undefined);
    t.assert.strictEqual(await stmt.get('key2', 'val2'), undefined);
    stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    t.assert.deepStrictEqual(await stmt.get(), { __proto__: null, key: 'key1', val: 'val1' });
  });

  test('executes a query that returns special columns', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('SELECT 1 as __proto__, 2 as constructor, 3 as toString');
    t.assert.deepStrictEqual(await stmt.get(), { __proto__: null, ['__proto__']: 1, constructor: 2, toString: 3 });
  });
});

suite('Statement.prototype.all()', () => {
  test('executes a query and returns an empty array on no results', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(await stmt.all(), []);
  });

  test('executes a query and returns all results', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(await stmt.run(), { changes: 0, lastInsertRowid: 0 });
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.deepStrictEqual(
      await stmt.run('key1', 'val1'),
      { changes: 1, lastInsertRowid: 1 },
    );
    t.assert.deepStrictEqual(
      await stmt.run('key2', 'val2'),
      { changes: 1, lastInsertRowid: 2 },
    );
    stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    t.assert.deepStrictEqual(await stmt.all(), [
      { __proto__: null, key: 'key1', val: 'val1' },
      { __proto__: null, key: 'key2', val: 'val2' },
    ]);
  });
});

suite('Statement.prototype.iterate()', () => {
  test('executes a query and returns an empty iterator on no results', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    const iter = stmt.iterate();
    t.assert.strictEqual(iter instanceof globalThis.Iterator, true);
    t.assert.ok(iter[Symbol.iterator]);
    t.assert.deepStrictEqual(iter.toArray(), []);
  });

  test('executes a query and returns all results', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(await stmt.run(), { changes: 0, lastInsertRowid: 0 });
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.deepStrictEqual(
      await stmt.run('key1', 'val1'),
      { changes: 1, lastInsertRowid: 1 },
    );
    t.assert.deepStrictEqual(
      await stmt.run('key2', 'val2'),
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

  test('iterator keeps the prepared statement from being collected', async (t) => {
    const db = new Database(':memory:');
    await db.exec(`
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

  test('iterator can be exited early', async (t) => {
    const db = new Database(':memory:');
    await db.exec(`
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

suite('Statement.prototype.run()', () => {
  test('executes a query and returns change metadata', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE storage(key TEXT, val TEXT);
      INSERT INTO storage (key, val) VALUES ('foo', 'bar');
    `);
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('SELECT * FROM storage');
    t.assert.deepStrictEqual(await stmt.run(), { changes: 1, lastInsertRowid: 1 });
  });

  test('SQLite throws when trying to bind too many parameters', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
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

  test('SQLite defaults to NULL for unbound parameters', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    await t.assert.rejects(
      stmt.run(1),
      {
        code: 'ERR_SQLITE_ERROR',
        message: 'NOT NULL constraint failed: data.val',
        errcode: 1299,
        errstr: 'constraint failed',
      });
  });

  test('returns correct metadata when using RETURNING', async (t) => {
    const db = new Database(':memory:');
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO data (key, val) VALUES ($k, $v) RETURNING key';
    const stmt = db.prepare(sql);
    t.assert.deepStrictEqual(
      await stmt.run({ k: 1, v: 10 }), { changes: 1, lastInsertRowid: 1 }
    );
    t.assert.deepStrictEqual(
      await stmt.run({ k: 2, v: 20 }), { changes: 1, lastInsertRowid: 2 }
    );
    t.assert.deepStrictEqual(
      await stmt.run({ k: 3, v: 30 }), { changes: 1, lastInsertRowid: 3 }
    );
  });

  test('SQLite defaults unbound ?NNN parameters', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?1, ?3)');

    await t.assert.rejects(
      stmt.run(1),
      {
        code: 'ERR_SQLITE_ERROR',
        message: 'NOT NULL constraint failed: data.val',
        errcode: 1299,
        errstr: 'constraint failed',
      });
  });

  test('binds ?NNN params by position', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?1, ?2)');
    t.assert.deepStrictEqual(await stmt.run(1, 2), { changes: 1, lastInsertRowid: 1 });
  });
});

suite('Statement.prototype.sourceSQL', () => {
  test('equals input SQL', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO types (key, val) VALUES ($k, $v)';
    const stmt = db.prepare(sql);
    t.assert.strictEqual(stmt.sourceSQL, sql);
  });
});

suite('Statement.prototype.expandedSQL', async () => {
  test('equals expanded SQL', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO types (key, val) VALUES ($k, ?)';
    const expanded = 'INSERT INTO types (key, val) VALUES (\'33\', \'42\')';
    const stmt = db.prepare(sql);
    t.assert.deepStrictEqual(
      await stmt.run({ $k: '33' }, '42'),
      { changes: 1, lastInsertRowid: 33 },
    );
    t.assert.strictEqual(stmt.expandedSQL, expanded);
  });
});

suite('Statement.prototype.setReadBigInts()', () => {
  test('BigInts support can be toggled', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data');
    t.assert.deepStrictEqual(await query.get(), { __proto__: null, val: 42 });
    t.assert.strictEqual(query.setReadBigInts(true), undefined);
    t.assert.deepStrictEqual(await query.get(), { __proto__: null, val: 42n });
    t.assert.strictEqual(query.setReadBigInts(false), undefined);
    t.assert.deepStrictEqual(await query.get(), { __proto__: null, val: 42 });

    const insert = db.prepare('INSERT INTO data (key) VALUES (?)');
    t.assert.deepStrictEqual(
      await insert.run(10),
      { changes: 1, lastInsertRowid: 10 },
    );
    t.assert.strictEqual(insert.setReadBigInts(true), undefined);
    t.assert.deepStrictEqual(
      await insert.run(20),
      { changes: 1n, lastInsertRowid: 20n },
    );
    t.assert.strictEqual(insert.setReadBigInts(false), undefined);
    t.assert.deepStrictEqual(
      await insert.run(30),
      { changes: 1, lastInsertRowid: 30 },
    );
  });

  test('throws when input is not a boolean', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO types (key, val) VALUES ($k, $v)');
    t.assert.throws(() => {
      stmt.setReadBigInts();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "readBigInts" argument must be a boolean/,
    });
  });

  test('BigInt is required for reading large integers', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const bad = db.prepare(`SELECT ${Number.MAX_SAFE_INTEGER} + 1`);
    await t.assert.rejects(
      bad.get(),
      {
        code: 'ERR_OUT_OF_RANGE',
        message: /^Value is too large to be represented as a JavaScript number: 9007199254740992$/,
      });
    const good = db.prepare(`SELECT ${Number.MAX_SAFE_INTEGER} + 1`);
    good.setReadBigInts(true);
    t.assert.deepStrictEqual(await good.get(), {
      __proto__: null,
      [`${Number.MAX_SAFE_INTEGER} + 1`]: 2n ** 53n,
    });
  });
});

suite('Statement.prototype.setReturnArrays()', () => {
  test('throws when input is not a boolean', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('SELECT key, val FROM data');
    t.assert.throws(() => {
      stmt.setReturnArrays();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "returnArrays" argument must be a boolean/,
    });
  });
});

suite('Statement.prototype.get() with array output', () => {
  test('returns array row when setReturnArrays is true', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data WHERE key = 1');
    t.assert.deepStrictEqual(await query.get(), { __proto__: null, key: 1, val: 'one' });

    query.setReturnArrays(true);
    t.assert.deepStrictEqual(await query.get(), [1, 'one']);

    query.setReturnArrays(false);
    t.assert.deepStrictEqual(await query.get(), { __proto__: null, key: 1, val: 'one' });
  });

  test('returns array rows with BigInts when both flags are set', async (t) => {
    const expected = [1n, 9007199254740992n];
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE big_data(id INTEGER, big_num INTEGER);
      INSERT INTO big_data VALUES (1, 9007199254740992);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT id, big_num FROM big_data');
    query.setReturnArrays(true);
    query.setReadBigInts(true);

    const row = await query.get();
    t.assert.deepStrictEqual(row, expected);
  });
});

suite('Statement.prototype.all() with array output', () => {
  test('returns array rows when setReturnArrays is true', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data ORDER BY key');
    t.assert.deepStrictEqual(await query.all(), [
      { __proto__: null, key: 1, val: 'one' },
      { __proto__: null, key: 2, val: 'two' },
    ]);

    query.setReturnArrays(true);
    t.assert.deepStrictEqual(await query.all(), [
      [1, 'one'],
      [2, 'two'],
    ]);

    query.setReturnArrays(false);
    t.assert.deepStrictEqual(await query.all(), [
      { __proto__: null, key: 1, val: 'one' },
      { __proto__: null, key: 2, val: 'two' },
    ]);
  });

  test('handles array rows with many columns', async (t) => {
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
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
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

    const query = db.prepare('SELECT * FROM wide_table');
    query.setReturnArrays(true);

    const results = await query.all();
    t.assert.strictEqual(results.length, 1);
    t.assert.deepStrictEqual(results[0], expected);
  });
});

suite('Statement.prototype.iterate() with array output', () => {
  test('iterates array rows when setReturnArrays is true', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data ORDER BY key');

    // Test with objects first
    const objectRows = [];
    for (const row of query.iterate()) {
      objectRows.push(row);
    }
    t.assert.deepStrictEqual(objectRows, [
      { __proto__: null, key: 1, val: 'one' },
      { __proto__: null, key: 2, val: 'two' },
    ]);

    // Test with arrays
    query.setReturnArrays(true);
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

  test('iterator can be exited early with array rows', async (t) => {
    const db = new Database(':memory:');
    await db.exec(`
      CREATE TABLE test(key TEXT, val TEXT);
      INSERT INTO test (key, val) VALUES ('key1', 'val1');
      INSERT INTO test (key, val) VALUES ('key2', 'val2');
    `);
    const stmt = db.prepare('SELECT key, val FROM test');
    stmt.setReturnArrays(true);

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

suite('Statement.prototype.setAllowBareNamedParameters()', () => {
  test('bare named parameter support can be toggled', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    t.assert.deepStrictEqual(
      await stmt.run({ k: 1, v: 2 }),
      { changes: 1, lastInsertRowid: 1 },
    );
    t.assert.strictEqual(stmt.setAllowBareNamedParameters(false), undefined);
    t.assert.throws(() => {
      stmt.run({ k: 2, v: 4 });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Unknown named parameter 'k'/,
    });
    t.assert.strictEqual(stmt.setAllowBareNamedParameters(true), undefined);
    t.assert.deepStrictEqual(
      await stmt.run({ k: 3, v: 6 }),
      { changes: 1, lastInsertRowid: 3 },
    );
  });

  test('throws when input is not a boolean', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    t.assert.throws(() => {
      stmt.setAllowBareNamedParameters();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "allowBareNamedParameters" argument must be a boolean/,
    });
  });
});
