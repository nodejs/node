// Flags: --expose-gc
'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const { DatabaseSync, StatementSync } = require('node:sqlite');
const { suite, test } = require('node:test');

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
    using db = new DatabaseSync(':memory:');
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.strictEqual(stmt.get(), undefined);
    stmt = db.prepare('SELECT * FROM storage');
    t.assert.strictEqual(stmt.get(), undefined);
  });

  test('executes a query and returns the first result', (t) => {
    using db = new DatabaseSync(':memory:');
    let stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.strictEqual(stmt.get(), undefined);
    stmt = db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)');
    t.assert.strictEqual(stmt.get('key1', 'val1'), undefined);
    t.assert.strictEqual(stmt.get('key2', 'val2'), undefined);
    stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, key: 'key1', val: 'val1' });
  });

  test('executes a query that returns special columns', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1 as __proto__, 2 as constructor, 3 as toString');
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, ['__proto__']: 1, constructor: 2, toString: 3 });
  });

  test('reflects an added column after the schema changes', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT)');
    db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)').run('key1', 'val1');
    const stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    db.exec("ALTER TABLE storage ADD COLUMN extra TEXT DEFAULT 'def'");
    t.assert.deepStrictEqual(stmt.get(), {
      __proto__: null, key: 'key1', val: 'val1', extra: 'def',
    });
  });

  test('reflects a dropped column after the schema changes', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT, extra TEXT)');
    db.prepare('INSERT INTO storage (key, val, extra) VALUES (?, ?, ?)')
      .run('key1', 'val1', 'x');
    const stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    db.exec('ALTER TABLE storage DROP COLUMN extra');
    t.assert.deepStrictEqual(stmt.get(), {
      __proto__: null, key: 'key1', val: 'val1',
    });
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.get();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });

  test('surfaces a deferred SQLite error from reset() even though a row was already built', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec(`
      PRAGMA foreign_keys = ON;
      PRAGMA defer_foreign_keys = ON;
      CREATE TABLE parent(id INTEGER PRIMARY KEY);
      CREATE TABLE child(id INTEGER PRIMARY KEY, parent_id INTEGER REFERENCES parent(id));
    `);
    // The FK check is deferred until the implicit transaction commits, which
    // happens inside reset() here because RETURNING leaves the statement's
    // VDBE running after the row is produced.
    const stmt = db.prepare(
      'INSERT INTO child (parent_id) VALUES (999) RETURNING id'
    );
    t.assert.throws(() => {
      stmt.get();
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /FOREIGN KEY constraint failed/,
    });
  });
});

suite('StatementSync.prototype.all()', () => {
  test('executes a query and returns an empty array on no results', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    t.assert.deepStrictEqual(stmt.all(), []);
  });

  test('executes a query and returns all results', (t) => {
    using db = new DatabaseSync(':memory:');
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

  test('reflects an added column after the schema changes', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT)');
    db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)').run('key1', 'val1');
    const stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    db.exec("ALTER TABLE storage ADD COLUMN extra TEXT DEFAULT 'def'");
    t.assert.deepStrictEqual(stmt.all(), [
      { __proto__: null, key: 'key1', val: 'val1', extra: 'def' },
    ]);
  });

  test('reflects a dropped column after the schema changes', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT, extra TEXT)');
    db.prepare('INSERT INTO storage (key, val, extra) VALUES (?, ?, ?)')
      .run('key1', 'val1', 'x');
    const stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    db.exec('ALTER TABLE storage DROP COLUMN extra');
    t.assert.deepStrictEqual(stmt.all(), [
      { __proto__: null, key: 'key1', val: 'val1' },
    ]);
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.all();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });

  test('surfaces a deferred SQLite error from reset() even though the array was already built', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec(`
      PRAGMA foreign_keys = ON;
      PRAGMA defer_foreign_keys = ON;
      CREATE TABLE parent(id INTEGER PRIMARY KEY);
      CREATE TABLE child(id INTEGER PRIMARY KEY, parent_id INTEGER REFERENCES parent(id));
    `);
    const stmt = db.prepare(
      'INSERT INTO child (parent_id) VALUES (999) RETURNING id'
    );
    t.assert.throws(() => {
      stmt.all();
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: /FOREIGN KEY constraint failed/,
    });
  });
});

suite('StatementSync.prototype.iterate()', () => {
  test('executes a query and returns an empty iterator on no results', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    const iter = stmt.iterate();
    t.assert.strictEqual(iter instanceof globalThis.Iterator, true);
    t.assert.ok(iter[Symbol.iterator]);
    t.assert.deepStrictEqual(iter.toArray(), []);
  });

  test('executes a query and returns all results', (t) => {
    using db = new DatabaseSync(':memory:');
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

  test('reflects an added column after the schema changes', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT)');
    db.prepare('INSERT INTO storage (key, val) VALUES (?, ?)').run('key1', 'val1');
    const stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    db.exec("ALTER TABLE storage ADD COLUMN extra TEXT DEFAULT 'def'");
    t.assert.deepStrictEqual(stmt.iterate().toArray(), [
      { __proto__: null, key: 'key1', val: 'val1', extra: 'def' },
    ]);
  });

  test('reflects a dropped column after the schema changes', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT, extra TEXT)');
    db.prepare('INSERT INTO storage (key, val, extra) VALUES (?, ?, ?)')
      .run('key1', 'val1', 'x');
    const stmt = db.prepare('SELECT * FROM storage ORDER BY key');
    db.exec('ALTER TABLE storage DROP COLUMN extra');
    t.assert.deepStrictEqual(stmt.iterate().toArray(), [
      { __proto__: null, key: 'key1', val: 'val1' },
    ]);
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

  test('iterator is invalidated when statement is reset by get/all/run/iterate', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE test (value INTEGER NOT NULL)');
    for (let i = 0; i < 5; i++) {
      db.prepare('INSERT INTO test (value) VALUES (?)').run(i);
    }
    const stmt = db.prepare('SELECT * FROM test');

    // Invalidated by stmt.get()
    let it = stmt.iterate();
    it.next();
    stmt.get();
    t.assert.throws(() => { it.next(); }, {
      code: 'ERR_INVALID_STATE',
      message: /iterator was invalidated/,
    });

    // Invalidated by stmt.all()
    it = stmt.iterate();
    it.next();
    stmt.all();
    t.assert.throws(() => { it.next(); }, {
      code: 'ERR_INVALID_STATE',
      message: /iterator was invalidated/,
    });

    // Invalidated by stmt.run()
    it = stmt.iterate();
    it.next();
    stmt.run();
    t.assert.throws(() => { it.next(); }, {
      code: 'ERR_INVALID_STATE',
      message: /iterator was invalidated/,
    });

    // Invalidated by a new stmt.iterate()
    it = stmt.iterate();
    it.next();
    const it2 = stmt.iterate();
    t.assert.throws(() => { it.next(); }, {
      code: 'ERR_INVALID_STATE',
      message: /iterator was invalidated/,
    });

    // New iterator works fine
    t.assert.strictEqual(it2.next().done, false);

    // Reset on a different statement does NOT invalidate this iterator
    const stmt2 = db.prepare('SELECT * FROM test');
    it = stmt.iterate();
    it.next();
    stmt2.get();
    it.next();
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.iterate();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });

  test('does not replay results after the iterator is naturally exhausted', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE test(key TEXT);
      INSERT INTO test (key) VALUES ('key1');
    `);
    const it = db.prepare('SELECT * FROM test').iterate();
    t.assert.deepStrictEqual(it.next(), {
      __proto__: null, done: false, value: { __proto__: null, key: 'key1' },
    });
    t.assert.deepStrictEqual(
      it.next(), { __proto__: null, done: true, value: null });
    // Calling next() again on an exhausted iterator must keep reporting
    // done, not silently reset the statement and replay from row 1.
    t.assert.deepStrictEqual(
      it.next(), { __proto__: null, done: true, value: null });
  });

  test('propagates a pending exception when the loop body throws mid-iteration', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE test(key TEXT);
      INSERT INTO test (key) VALUES ('key1');
      INSERT INTO test (key) VALUES ('key2');
    `);
    const stmt = db.prepare('SELECT * FROM test');
    const userError = new Error('boom');
    t.assert.throws(() => {
      // eslint-disable-next-line no-unused-vars
      for (const row of stmt.iterate()) {
        throw userError;
      }
    }, (err) => err === userError);
  });
});

suite('StatementSync.prototype.run()', () => {
  test('executes a query and returns change metadata', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE storage(key TEXT, val TEXT);
      INSERT INTO storage (key, val) VALUES ('foo', 'bar');
    `);
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('SELECT * FROM storage');
    t.assert.deepStrictEqual(stmt.run(), { changes: 1, lastInsertRowid: 1 });
  });

  test('SQLite throws when trying to bind too many parameters', (t) => {
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER NOT NULL) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?1, ?2)');
    t.assert.deepStrictEqual(stmt.run(1, 2), { changes: 1, lastInsertRowid: 1 });
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.run();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.sourceSQL', () => {
  test('equals input SQL', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const sql = 'INSERT INTO types (key, val) VALUES ($k, $v)';
    const stmt = db.prepare(sql);
    t.assert.strictEqual(stmt.sourceSQL, sql);
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => stmt.sourceSQL, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.expandedSQL', () => {
  test('equals expanded SQL', (t) => {
    using db = new DatabaseSync(':memory:');
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

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => stmt.expandedSQL, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.stat()', () => {
  const counters = [
    'fullscanStep', 'sort', 'autoindex', 'vmStep', 'reprepare',
    'run', 'memused',
  ];

  // 'filterMiss' and 'filterHit' map to SQLITE_STMTSTATUS_FILTER_MISS and
  // SQLITE_STMTSTATUS_FILTER_HIT, which were added in SQLite 3.38.0. Builds
  // using an older shared SQLite do not expose them.
  const [major, minor] = process.versions.sqlite.split('.').map(Number);
  const hasFilterCounters = major > 3 || (major === 3 && minor >= 38);

  if (hasFilterCounters) {
    counters.push('filterMiss', 'filterHit');
  }

  test('returns a number for every valid counter', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;');
    const stmt = db.prepare('SELECT * FROM data');
    for (const counter of counters) {
      t.assert.strictEqual(typeof stmt.stat(counter), 'number');
    }
  });

  test('counts virtual machine steps and runs after execution', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;');
    const insert = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    for (let i = 1; i <= 5; i++) {
      insert.run(i, `val-${i}`);
    }
    const stmt = db.prepare('SELECT * FROM data');
    t.assert.strictEqual(stmt.stat('run'), 0);
    t.assert.strictEqual(stmt.stat('vmStep'), 0);
    stmt.all();
    t.assert.strictEqual(stmt.stat('run'), 1);
    t.assert.ok(stmt.stat('vmStep') > 0);
    t.assert.ok(stmt.stat('memused') > 0);
  });

  test('detects full table scans', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;');
    const insert = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    for (let i = 1; i <= 10; i++) {
      insert.run(i, `val-${i}`);
    }

    // Filtering on a non-indexed column forces a full table scan.
    const scan = db.prepare('SELECT * FROM data WHERE val = ?');
    scan.all('val-5');
    t.assert.ok(scan.stat('fullscanStep') > 0);

    // Filtering on the primary key uses the index; no full scan occurs.
    const indexed = db.prepare('SELECT * FROM data WHERE key = ?');
    indexed.all(5);
    t.assert.strictEqual(indexed.stat('fullscanStep'), 0);
  });

  test('reading a counter does not reset it', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;');
    const stmt = db.prepare('SELECT * FROM data');
    stmt.all();
    const first = stmt.stat('run');
    t.assert.strictEqual(stmt.stat('run'), first);
  });

  test('throws if the counter argument is not a string', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1');
    t.assert.throws(() => stmt.stat(), {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "counter" argument must be a string/,
    });
    t.assert.throws(() => stmt.stat(42), {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "counter" argument must be a string/,
    });
  });

  test('throws if the counter name is unknown', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1');
    t.assert.throws(() => stmt.stat('nope'), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /The "counter" argument is not a valid statistic name/,
    });
  });

  test('throws if the statement is finalized', (t) => {
    const db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1');
    db.close();
    t.assert.throws(() => stmt.stat('run'), {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.resetStats()', () => {
  test('returns undefined', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1');
    t.assert.strictEqual(stmt.resetStats(), undefined);
  });

  // The column name cache is keyed on the reprepare counter, which
  // resetStats() zeroes. A later re-prepare must not be able to make the
  // counter match the cached generation again and reuse stale names.
  test('invalidates cached iterator column names', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(a); INSERT INTO data VALUES (1)');
    const stmt = db.prepare('SELECT * FROM data');

    db.exec('ALTER TABLE data RENAME COLUMN a TO b');
    stmt.iterate().toArray();
    stmt.resetStats();
    db.exec('ALTER TABLE data RENAME COLUMN b TO c');

    t.assert.deepStrictEqual(stmt.iterate().toArray(), [
      { __proto__: null, c: 1 },
    ]);
  });

  test('invalidates the cache when the column count grows', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t(a); INSERT INTO t VALUES (1)');
    const stmt = db.prepare('SELECT * FROM t');

    db.exec('ALTER TABLE t ADD COLUMN b DEFAULT 2');
    stmt.iterate().toArray();
    stmt.resetStats();
    db.exec('ALTER TABLE t ADD COLUMN c DEFAULT 3');

    t.assert.deepStrictEqual(stmt.iterate().toArray(), [
      { __proto__: null, a: 1, b: 2, c: 3 },
    ]);
  });

  test('does not reset memused', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE t(a); INSERT INTO t VALUES (1),(2),(3)');
    const stmt = db.prepare('SELECT * FROM t ORDER BY a');
    stmt.all();

    // The memused counter reports current memory usage rather than an
    // accumulated total, so SQLite ignores the reset flag for it.
    const before = stmt.stat('memused');
    t.assert.ok(before > 0);
    stmt.resetStats();
    t.assert.strictEqual(stmt.stat('memused'), before);
  });

  test('clears every counter', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;');
    const insert = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    for (let i = 1; i <= 5; i++) {
      insert.run(i, `val-${i}`);
    }

    // Force a full table scan so more than one counter is non-zero.
    const stmt = db.prepare('SELECT * FROM data WHERE val = ?');
    stmt.all('val-3');
    t.assert.ok(stmt.stat('run') > 0);
    t.assert.ok(stmt.stat('vmStep') > 0);
    t.assert.ok(stmt.stat('fullscanStep') > 0);

    stmt.resetStats();
    t.assert.strictEqual(stmt.stat('run'), 0);
    t.assert.strictEqual(stmt.stat('vmStep'), 0);
    t.assert.strictEqual(stmt.stat('fullscanStep'), 0);
  });

  test('counters accumulate again after a reset', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;');
    const stmt = db.prepare('SELECT * FROM data');
    stmt.all();
    stmt.resetStats();
    t.assert.strictEqual(stmt.stat('run'), 0);
    stmt.all();
    t.assert.strictEqual(stmt.stat('run'), 1);
  });

  test('is a no-op when no counters have been incremented', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1');
    stmt.resetStats();
    t.assert.strictEqual(stmt.stat('run'), 0);
  });

  test('throws if the statement is finalized', (t) => {
    const db = new DatabaseSync(':memory:');
    const stmt = db.prepare('SELECT 1');
    db.close();
    t.assert.throws(() => stmt.resetStats(), {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.setReadBigInts()', () => {
  test('BigInts support can be toggled', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data');
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42 });
    t.assert.strictEqual(query.setReadBigInts(true), undefined);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42n });
    t.assert.strictEqual(query.setReadBigInts(false), undefined);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42 });

    const insert = db.prepare('INSERT INTO data (key) VALUES (?)');
    t.assert.deepStrictEqual(
      insert.run(10),
      { changes: 1, lastInsertRowid: 10 },
    );
    t.assert.strictEqual(insert.setReadBigInts(true), undefined);
    t.assert.deepStrictEqual(
      insert.run(20),
      { changes: 1n, lastInsertRowid: 20n },
    );
    t.assert.strictEqual(insert.setReadBigInts(false), undefined);
    t.assert.deepStrictEqual(
      insert.run(30),
      { changes: 1, lastInsertRowid: 30 },
    );
  });

  test('throws when input is not a boolean', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(
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

  test('BigInt is required for reading large integers', (t) => {
    using db = new DatabaseSync(':memory:');
    const bad = db.prepare(`SELECT ${Number.MAX_SAFE_INTEGER} + 1`);
    t.assert.throws(() => {
      bad.get();
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /^Value is too large to be represented as a JavaScript number: 9007199254740992$/,
    });
    const good = db.prepare(`SELECT ${Number.MAX_SAFE_INTEGER} + 1`);
    good.setReadBigInts(true);
    t.assert.deepStrictEqual(good.get(), {
      __proto__: null,
      [`${Number.MAX_SAFE_INTEGER} + 1`]: 2n ** 53n,
    });
  });

  test('BigInt is required for reading large last insert row IDs', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY) STRICT');
    const insert = db.prepare('INSERT INTO data VALUES (?)');

    t.assert.throws(() => {
      insert.run(9007199254740993n);
    }, {
      code: 'ERR_OUT_OF_RANGE',
      message: /^Value is too large to be represented as a JavaScript number: 9007199254740993$/,
    });

    insert.setReadBigInts(true);
    t.assert.deepStrictEqual(insert.run(9007199254740995n), {
      changes: 1n,
      lastInsertRowid: 9007199254740995n,
    });
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.setReadBigInts(true);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.setReturnArrays()', () => {
  test('throws when input is not a boolean', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(
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

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.setReturnArrays(true);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype.get() with array output', () => {
  test('returns array row when setReturnArrays is true', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data WHERE key = 1');
    t.assert.deepStrictEqual(query.get(), { __proto__: null, key: 1, val: 'one' });

    query.setReturnArrays(true);
    t.assert.deepStrictEqual(query.get(), [1, 'one']);

    query.setReturnArrays(false);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, key: 1, val: 'one' });
  });

  test('returns array rows with BigInts when both flags are set', (t) => {
    const expected = [1n, 9007199254740992n];
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE big_data(id INTEGER, big_num INTEGER);
      INSERT INTO big_data VALUES (1, 9007199254740992);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT id, big_num FROM big_data');
    query.setReturnArrays(true);
    query.setReadBigInts(true);

    const row = query.get();
    t.assert.deepStrictEqual(row, expected);
  });
});

suite('StatementSync.prototype.all() with array output', () => {
  test('returns array rows when setReturnArrays is true', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val TEXT) STRICT;
      INSERT INTO data (key, val) VALUES (1, 'one');
      INSERT INTO data (key, val) VALUES (2, 'two');
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT key, val FROM data ORDER BY key');
    t.assert.deepStrictEqual(query.all(), [
      { __proto__: null, key: 1, val: 'one' },
      { __proto__: null, key: 2, val: 'two' },
    ]);

    query.setReturnArrays(true);
    t.assert.deepStrictEqual(query.all(), [
      [1, 'one'],
      [2, 'two'],
    ]);

    query.setReturnArrays(false);
    t.assert.deepStrictEqual(query.all(), [
      { __proto__: null, key: 1, val: 'one' },
      { __proto__: null, key: 2, val: 'two' },
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
    using db = new DatabaseSync(':memory:');
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

    const query = db.prepare('SELECT * FROM wide_table');
    query.setReturnArrays(true);

    const results = query.all();
    t.assert.strictEqual(results.length, 1);
    t.assert.deepStrictEqual(results[0], expected);
  });
});

suite('StatementSync.prototype.iterate() with array output', () => {
  test('iterates array rows when setReturnArrays is true', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
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

  test('iterator can be exited early with array rows', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
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

suite('StatementSync.prototype.setAllowBareNamedParameters()', () => {
  test('bare named parameter support can be toggled', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    t.assert.deepStrictEqual(
      stmt.run({ k: 1, v: 2 }),
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
      stmt.run({ k: 3, v: 6 }),
      { changes: 1, lastInsertRowid: 3 },
    );
  });

  test('throws when input is not a boolean', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(
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

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.setAllowBareNamedParameters(true);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('options.readBigInts', () => {
  test('BigInts are returned when input is true', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data', { readBigInts: true });
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42n });
  });

  test('numbers are returned when input is false', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data', { readBigInts: false });
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42 });
  });

  test('throws when input is not a boolean', (t) => {
    using db = new DatabaseSync(':memory:');
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

  test('setReadBigInts can override prepare option', (t) => {
    using db = new DatabaseSync(':memory:');
    const setup = db.exec(`
      CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;
      INSERT INTO data (key, val) VALUES (1, 42);
    `);
    t.assert.strictEqual(setup, undefined);

    const query = db.prepare('SELECT val FROM data', { readBigInts: true });
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42n });
    t.assert.strictEqual(query.setReadBigInts(false), undefined);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, val: 42 });
  });
});

suite('options.returnArrays', () => {
  test('arrays are returned when input is true', (t) => {
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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

  test('setReturnArrays can override prepare option', (t) => {
    using db = new DatabaseSync(':memory:');
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
    t.assert.strictEqual(query.setReturnArrays(false), undefined);
    t.assert.deepStrictEqual(query.get(), { __proto__: null, key: 1, val: 'one' });
  });

  test('all() returns arrays when input is true', (t) => {
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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
    using db = new DatabaseSync(':memory:');
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

  test('setAllowBareNamedParameters can override prepare option', (t) => {
    using db = new DatabaseSync(':memory:');
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
    t.assert.strictEqual(stmt.setAllowBareNamedParameters(true), undefined);
    t.assert.deepStrictEqual(
      stmt.run({ k: 2, v: 4 }),
      { changes: 1, lastInsertRowid: 2 },
    );
  });
});


suite('StatementSync.prototype.close()', () => {
  test('finalizes an open statement', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT)');
    const stmt = db.prepare('SELECT * FROM storage');
    t.assert.strictEqual(stmt.close(), undefined);
    t.assert.throws(() => stmt.get(), {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });

  test('throws if the statement is already finalized', (t) => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt.close();
    t.assert.throws(() => {
      stmt.close();
    }, {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });
});

suite('StatementSync.prototype[Symbol.dispose]()', () => {
  test('finalizes an open statement', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT)');
    const stmt = db.prepare('SELECT * FROM storage');
    stmt[Symbol.dispose]();
    t.assert.throws(() => stmt.get(), {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });

  test('does not throw on an already-finalized statement', () => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt[Symbol.dispose]();
    stmt[Symbol.dispose]();
  });

  test('works with a using declaration', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE storage(key TEXT, val TEXT)');
    let captured;
    {
      using stmt = db.prepare('SELECT * FROM storage');
      captured = stmt;
      t.assert.deepStrictEqual(stmt.all(), []);
    }
    t.assert.throws(() => captured.get(), {
      code: 'ERR_INVALID_STATE',
      message: /statement has been finalized/,
    });
  });

  test('closing the database after dispose does not double-finalize', () => {
    using db = new DatabaseSync(':memory:');
    const stmt = db.prepare('CREATE TABLE storage(key TEXT, val TEXT)');
    stmt[Symbol.dispose]();
    db.close();
  });
});

suite('options.persistent', () => {
  test('statement executes correctly when persistent is true', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;');
    db.exec('INSERT INTO data (key, val) VALUES (1, 42);');
    using stmt = db.prepare('SELECT val FROM data', { persistent: true });
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, val: 42 });
  });

  test('statement executes correctly when persistent is false', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;');
    db.exec('INSERT INTO data (key, val) VALUES (1, 42);');
    using stmt = db.prepare('SELECT val FROM data', { persistent: false });
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, val: 42 });
  });

  test('throws when input is not a boolean', (t) => {
    using db = new DatabaseSync(':memory:');
    t.assert.throws(() => {
      db.prepare('SELECT 1', { persistent: 'yes' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.persistent" argument must be a boolean/,
    });
  });

  test('can be combined with other options', (t) => {
    using db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;');
    db.exec('INSERT INTO data (key, val) VALUES (1, 42);');
    using stmt = db.prepare(
      'SELECT val FROM data',
      { persistent: true, readBigInts: true }
    );
    t.assert.deepStrictEqual(stmt.get(), { __proto__: null, val: 42n });
  });
});
