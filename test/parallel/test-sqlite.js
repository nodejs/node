'use strict';
const { spawnPromisified } = require('../common');
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { DatabaseSync, constants } = require('node:sqlite');
const { suite, test } = require('node:test');
const { pathToFileURL } = require('node:url');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('accessing the node:sqlite module', () => {
  test('cannot be accessed without the node: scheme', (t) => {
    t.assert.throws(() => {
      require('sqlite');
    }, {
      code: 'MODULE_NOT_FOUND',
      message: /Cannot find module 'sqlite'/,
    });
  });

  test('can be disabled with --no-experimental-sqlite flag', async (t) => {
    const {
      stdout,
      stderr,
      code,
      signal,
    } = await spawnPromisified(process.execPath, [
      '--no-experimental-sqlite',
      '-e',
      'require("node:sqlite")',
    ]);

    t.assert.strictEqual(stdout, '');
    t.assert.match(stderr, /No such built-in module: node:sqlite/);
    t.assert.notStrictEqual(code, 0);
    t.assert.strictEqual(signal, null);
  });
});

test('ERR_SQLITE_ERROR is thrown for errors originating from SQLite', (t) => {
  const db = new DatabaseSync(nextDb());
  t.after(() => { db.close(); });
  const setup = db.exec(`
    CREATE TABLE test(
      key INTEGER PRIMARY KEY
    ) STRICT;
  `);
  t.assert.strictEqual(setup, undefined);
  const stmt = db.prepare('INSERT INTO test (key) VALUES (?)');
  t.assert.deepStrictEqual(stmt.run(1), { changes: 1, lastInsertRowid: 1 });
  t.assert.throws(() => {
    stmt.run(1);
  }, {
    code: 'ERR_SQLITE_ERROR',
    message: 'UNIQUE constraint failed: test.key',
    errcode: 1555,
    errstr: 'constraint failed',
  });
});

test('in-memory databases are supported', (t) => {
  const db1 = new DatabaseSync(':memory:');
  const db2 = new DatabaseSync(':memory:');
  const setup1 = db1.exec(`
    CREATE TABLE data(key INTEGER PRIMARY KEY);
    INSERT INTO data (key) VALUES (1);
  `);
  const setup2 = db2.exec(`
    CREATE TABLE data(key INTEGER PRIMARY KEY);
    INSERT INTO data (key) VALUES (1);
  `);
  t.assert.strictEqual(setup1, undefined);
  t.assert.strictEqual(setup2, undefined);
  t.assert.deepStrictEqual(
    db1.prepare('SELECT * FROM data').all(),
    [{ __proto__: null, key: 1 }]
  );
  t.assert.deepStrictEqual(
    db2.prepare('SELECT * FROM data').all(),
    [{ __proto__: null, key: 1 }]
  );
});

test('sqlite constants are defined', (t) => {
  t.assert.strictEqual(constants.SQLITE_CHANGESET_OMIT, 0);
  t.assert.strictEqual(constants.SQLITE_CHANGESET_REPLACE, 1);
  t.assert.strictEqual(constants.SQLITE_CHANGESET_ABORT, 2);
});

test('PRAGMAs are supported', (t) => {
  const db = new DatabaseSync(nextDb());
  t.after(() => { db.close(); });
  t.assert.deepStrictEqual(
    db.prepare('PRAGMA journal_mode = WAL').get(),
    { __proto__: null, journal_mode: 'wal' },
  );
  t.assert.deepStrictEqual(
    db.prepare('PRAGMA journal_mode').get(),
    { __proto__: null, journal_mode: 'wal' },
  );
});

test('Buffer is supported as the database path', (t) => {
  const db = new DatabaseSync(Buffer.from(nextDb()));
  t.after(() => { db.close(); });
  db.exec(`
    CREATE TABLE data(key INTEGER PRIMARY KEY);
    INSERT INTO data (key) VALUES (1);
  `);

  t.assert.deepStrictEqual(
    db.prepare('SELECT * FROM data').all(),
    [{ __proto__: null, key: 1 }]
  );
});

test('URL is supported as the database path', (t) => {
  const url = pathToFileURL(nextDb());
  const db = new DatabaseSync(url);
  t.after(() => { db.close(); });
  db.exec(`
    CREATE TABLE data(key INTEGER PRIMARY KEY);
    INSERT INTO data (key) VALUES (1);
  `);

  t.assert.deepStrictEqual(
    db.prepare('SELECT * FROM data').all(),
    [{ __proto__: null, key: 1 }]
  );
});

suite('URI query params', () => {
  const baseDbPath = nextDb();
  const baseDb = new DatabaseSync(baseDbPath);
  baseDb.exec(`
    CREATE TABLE data(key INTEGER PRIMARY KEY);
    INSERT INTO data (key) VALUES (1);
  `);
  baseDb.close();

  test('query params are supported with URL objects', (t) => {
    const url = pathToFileURL(baseDbPath);
    url.searchParams.set('mode', 'ro');
    const readOnlyDB = new DatabaseSync(url);
    t.after(() => { readOnlyDB.close(); });

    t.assert.deepStrictEqual(
      readOnlyDB.prepare('SELECT * FROM data').all(),
      [{ __proto__: null, key: 1 }]
    );
    t.assert.throws(() => {
      readOnlyDB.exec('INSERT INTO data (key) VALUES (1);');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'attempt to write a readonly database',
    });
  });

  test('query params are supported with string', (t) => {
    const url = pathToFileURL(baseDbPath);
    url.searchParams.set('mode', 'ro');

    // Ensures a valid URI passed as a string is supported
    const readOnlyDB = new DatabaseSync(url.toString());
    t.after(() => { readOnlyDB.close(); });

    t.assert.deepStrictEqual(
      readOnlyDB.prepare('SELECT * FROM data').all(),
      [{ __proto__: null, key: 1 }]
    );
    t.assert.throws(() => {
      readOnlyDB.exec('INSERT INTO data (key) VALUES (1);');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'attempt to write a readonly database',
    });
  });

  test('query params are supported with Buffer', (t) => {
    const url = pathToFileURL(baseDbPath);
    url.searchParams.set('mode', 'ro');

    // Ensures a valid URI passed as a Buffer is supported
    const readOnlyDB = new DatabaseSync(Buffer.from(url.toString()));
    t.after(() => { readOnlyDB.close(); });

    t.assert.deepStrictEqual(
      readOnlyDB.prepare('SELECT * FROM data').all(),
      [{ __proto__: null, key: 1 }]
    );
    t.assert.throws(() => {
      readOnlyDB.exec('INSERT INTO data (key) VALUES (1);');
    }, {
      code: 'ERR_SQLITE_ERROR',
      message: 'attempt to write a readonly database',
    });
  });
});

suite('SQL APIs enabled at build time', () => {
  test('math functions are enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.deepStrictEqual(
      db.prepare('SELECT PI() AS pi').get(),
      { __proto__: null, pi: 3.141592653589793 },
    );
  });

  test('dbstat is enabled', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    db.exec(`
      CREATE TABLE t1 (key INTEGER PRIMARY KEY);
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM dbstat WHERE name = \'t1\'').get(),
      {
        __proto__: null,
        mx_payload: 0,
        name: 't1',
        ncell: 0,
        pageno: 2,
        pagetype: 'leaf',
        path: '/',
        payload: 0,
        pgoffset: 4096,
        pgsize: 4096,
        unused: 4088
      },
    );
  });

  test('fts3 is enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE VIRTUAL TABLE t1 USING fts3(content TEXT);
      INSERT INTO t1 (content) VALUES ('hello world');
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM t1 WHERE t1 MATCH \'hello\'').all(),
      [
        { __proto__: null, content: 'hello world' },
      ],
    );
  });

  test('fts3 parenthesis', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE VIRTUAL TABLE t1 USING fts3(content TEXT);
      INSERT INTO t1 (content) VALUES ('hello world');
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM t1 WHERE content MATCH \'(groupedterm1 OR groupedterm2) OR hello world\'').all(),
      [
        { __proto__: null, content: 'hello world' },
      ],
    );
  });

  test('fts4 is enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE VIRTUAL TABLE t1 USING fts4(content TEXT);
      INSERT INTO t1 (content) VALUES ('hello world');
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM t1 WHERE t1 MATCH \'hello\'').all(),
      [
        { __proto__: null, content: 'hello world' },
      ],
    );
  });

  test('fts5 is enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE VIRTUAL TABLE t1 USING fts5(content);
      INSERT INTO t1 (content) VALUES ('hello world');
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM t1 WHERE t1 MATCH \'hello\'').all(),
      [
        { __proto__: null, content: 'hello world' },
      ],
    );
  });

  test('rtree is enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE VIRTUAL TABLE t1 USING rtree(id, minX, maxX, minY, maxY);
      INSERT INTO t1 (id, minX, maxX, minY, maxY) VALUES (1, 0, 1, 0, 1);
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM t1 WHERE minX < 0.5').all(),
      [
        { __proto__: null, id: 1, minX: 0, maxX: 1, minY: 0, maxY: 1 },
      ],
    );
  });

  test('rbu is enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    t.assert.deepStrictEqual(
      db.prepare('SELECT sqlite_compileoption_used(\'SQLITE_ENABLE_RBU\') as rbu_enabled;').get(),
      { __proto__: null, rbu_enabled: 1 },
    );
  });

  test('geopoly is enabled', (t) => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE VIRTUAL TABLE t1 USING geopoly(a,b,c);
      INSERT INTO t1(_shape) VALUES('[[0,0],[1,0],[0.5,1],[0,0]]');
    `);

    t.assert.deepStrictEqual(
      db.prepare('SELECT rowid FROM t1 WHERE geopoly_contains_point(_shape, 0, 0)').get(),
      { __proto__: null, rowid: 1 },
    );
  });
});
