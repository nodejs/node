'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');
let cnt = 0;

tmpdir.refresh();

function nextDb() {
  return join(tmpdir.path, `database-${cnt++}.db`);
}

suite('named parameters', () => {
  test('throws on unknown named parameters', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);

    t.assert.throws(() => {
      const stmt = db.prepare('INSERT INTO types (key, val) VALUES ($k, $v)');
      stmt.run({ $k: 1, $unknown: 1 });
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Unknown named parameter '\$unknown'/,
    });
  });

  test('bare named parameters are supported', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    stmt.run({ k: 1, v: 9 });
    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM data').get(),
      { __proto__: null, key: 1, val: 9 },
    );
  });

  test('duplicate bare named parameters are supported', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $k)');
    stmt.run({ k: 1 });
    t.assert.deepStrictEqual(
      db.prepare('SELECT * FROM data').get(),
      { __proto__: null, key: 1, val: 1 },
    );
  });

  test('bare named parameters throw on ambiguous names', (t) => {
    const db = new DatabaseSync(nextDb());
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE types(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO types (key, val) VALUES ($k, @k)');
    t.assert.throws(() => {
      stmt.run({ k: 1 });
    }, {
      code: 'ERR_INVALID_STATE',
      message: 'Cannot create bare named parameter \'k\' because of ' +
               'conflicting names \'$k\' and \'@k\'.',
    });
  });
});

suite('StatementSync.prototype.setAllowUnknownNamedParameters()', () => {
  test('unknown named parameter support can be toggled', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    t.assert.strictEqual(stmt.setAllowUnknownNamedParameters(true), undefined);
    const params = { $a: 1, $b: 2, $k: 42, $y: 25, $v: 84, $z: 99 };
    t.assert.deepStrictEqual(
      stmt.run(params),
      { changes: 1, lastInsertRowid: 1 },
    );
    t.assert.strictEqual(stmt.setAllowUnknownNamedParameters(false), undefined);
    t.assert.throws(() => {
      stmt.run(params);
    }, {
      code: 'ERR_INVALID_STATE',
      message: /Unknown named parameter '\$a'/,
    });
  });

  test('throws when input is not a boolean', (t) => {
    const db = new DatabaseSync(':memory:');
    t.after(() => { db.close(); });
    const setup = db.exec(
      'CREATE TABLE data(key INTEGER PRIMARY KEY, val INTEGER) STRICT;'
    );
    t.assert.strictEqual(setup, undefined);
    const stmt = db.prepare('INSERT INTO data (key, val) VALUES ($k, $v)');
    t.assert.throws(() => {
      stmt.setAllowUnknownNamedParameters();
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "enabled" argument must be a boolean/,
    });
  });
});
