'use strict';
const { skipIfSQLiteMissing } = require('../common');
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
    const r = await stmt.run();
    t.assert.deepStrictEqual(r, { changes: 1, lastInsertRowid: 1 });
  });

  test('SQLite throws when trying to bind too many parameters', async (t) => {
    const db = new Database(nextDb());
    t.after(() => { db.close(); });
    const setup = await db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES (?, ?)');
    t.assert.rejects(async () => {
      await stmt.run(1, 2, 3);
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
    await t.assert.rejects(async () => {
      await stmt.run(1);
    }, {
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
    let r = await stmt.run({ k: 1, v: 10 });
    t.assert.deepStrictEqual(r, { changes: 1, lastInsertRowid: 1 });

    r = await stmt.run({ k: 2, v: 20 });
    t.assert.deepStrictEqual(r, { changes: 1, lastInsertRowid: 2 });

    r = await stmt.run({ k: 3, v: 30 });
    t.assert.deepStrictEqual(r, { changes: 1, lastInsertRowid: 3 });
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
