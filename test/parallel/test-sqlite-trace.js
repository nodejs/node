'use strict';

const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();

const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { DatabaseSync } = require('node:sqlite');
const { suite, it } = require('node:test');

suite('sqlite.db.query diagnostics channel', () => {
  it('subscriber receives SQL string for exec() statements', (t) => {
    const calls = [];
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');

    assert.strictEqual(calls.length, 2);
    assert.strictEqual(calls[0], 'CREATE TABLE t (x INTEGER)');
    assert.strictEqual(calls[1], 'INSERT INTO t VALUES (1)');
  });

  it('subscriber receives SQL string for prepared INSERT statements', (t) => {
    let calls = [];
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    calls = []; // reset after setup

    const stmt = db.prepare('INSERT INTO t VALUES (?)');
    stmt.run(42);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0], 'INSERT INTO t VALUES (42.0)');
  });

  it('subscriber receives SQL string for prepared SELECT statements', (t) => {
    let calls = [];
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    calls = []; // reset after setup

    const stmt = db.prepare('SELECT x FROM t WHERE x = ?');
    stmt.get(1);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0], 'SELECT x FROM t WHERE x = 1.0');
  });

  it('subscriber receives SQL string for prepared UPDATE statements', (t) => {
    let calls = [];
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    calls = []; // reset after setup

    const stmt = db.prepare('UPDATE t SET x = ? WHERE x = ?');
    stmt.run(2, 1);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0], 'UPDATE t SET x = 2.0 WHERE x = 1.0');
  });

  it('subscriber receives SQL string for prepared DELETE statements', (t) => {
    let calls = [];
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x INTEGER)');
    db.exec('INSERT INTO t VALUES (1)');
    calls = []; // reset after setup

    const stmt = db.prepare('DELETE FROM t WHERE x = ?');
    stmt.run(1);

    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0], 'DELETE FROM t WHERE x = 1.0');
  });

  it('no calls received after unsubscribe', (t) => {
    const calls = [];
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);

    db.exec('CREATE TABLE t (x INTEGER)');
    assert.strictEqual(calls.length, 1);

    dc.unsubscribe('sqlite.db.query', handler);
    db.exec('INSERT INTO t VALUES (1)');
    assert.strictEqual(calls.length, 1); // No new calls after unsubscribe
  });

  it('falls back to source SQL when expansion fails', (t) => {
    let calls = [];
    const db = new DatabaseSync(':memory:', { limits: { length: 1000 } });
    t.after(() => db.close());

    const handler = (sql) => calls.push(sql);
    dc.subscribe('sqlite.db.query', handler);
    t.after(() => dc.unsubscribe('sqlite.db.query', handler));

    db.exec('CREATE TABLE t (x TEXT)');
    calls = []; // reset after setup

    const stmt = db.prepare('INSERT INTO t VALUES (?)');

    const longValue = 'a'.repeat(977);
    stmt.run(longValue);

    assert.strictEqual(calls.length, 1);
    // Falls back to source SQL with unexpanded '?' placeholder
    assert.strictEqual(calls[0], 'INSERT INTO t VALUES (?)');
  });
});
