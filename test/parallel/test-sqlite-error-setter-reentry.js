'use strict';

const { skipIfSQLiteMissing, mustCall } = require('../common');
skipIfSQLiteMissing();
const assert = require('node:assert');
const { test } = require('node:test');
const { Database } = require('node:sqlite');

// Setting `code` on a thrown error runs Error.prototype setters. Closing the
// database there must not crash run() or get().
function closeFromCodeSetter(db, fn) {
  Object.defineProperty(Error.prototype, 'code', {
    __proto__: null,
    configurable: true,
    get: undefined,
    set: mustCall(() => {
      delete Error.prototype.code;
      db.close();
    }),
  });
  try {
    return fn();
  } finally {
    delete Error.prototype.code;
  }
}

const cases = [
  {
    name: 'statement.run() failing in sqlite3_step()',
    sql: 'INSERT INTO t VALUES (1)',
    call: (db, sql) => db.prepare(sql).run(),
    error: { message: 'UNIQUE constraint failed: t.x' },
  },
  {
    name: 'statement.get() failing to convert a row',
    sql: 'SELECT 9007199254740993',
    call: (db, sql) => db.prepare(sql).get(),
    error: { name: 'RangeError', message: /too large/ },
  },
  {
    name: 'tag store run() failing in sqlite3_step()',
    call: (db) => db.createTagStore().run`INSERT INTO t VALUES (1)`,
    error: { message: 'UNIQUE constraint failed: t.x' },
  },
  {
    name: 'tag store get() failing to convert a row',
    call: (db) => db.createTagStore().get`SELECT 9007199254740993`,
    error: { name: 'RangeError', message: /too large/ },
  },
];

for (const { name, sql, call, error } of cases) {
  test(`database.close() from an error setter during ${name}`, () => {
    const db = new Database(':memory:');
    db.exec('CREATE TABLE t (x UNIQUE); INSERT INTO t VALUES (1);');

    assert.throws(() => closeFromCodeSetter(db, () => call(db, sql)), error);
    assert.strictEqual(db.isOpen, false);
  });
}
