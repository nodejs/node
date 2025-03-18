'use strict';
require('../common');
const assert = require('node:assert');
const { DatabaseSync } = require('node:sqlite');
const { suite, test } = require('node:test');

suite('StatementSync.prototype.columns()', () => {
  test('returns column metadata for core SQLite types', () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`CREATE TABLE test (
      col1 INTEGER,
      col2 REAL,
      col3 TEXT,
      col4 BLOB,
      col5 NULL
    )`);
    const stmt = db.prepare('SELECT col1, col2, col3, col4, col5 FROM test');
    assert.deepStrictEqual(stmt.columns(), [
      {
        __proto__: null,
        column: 'col1',
        database: 'main',
        name: 'col1',
        table: 'test',
        type: 'INTEGER',
      },
      {
        __proto__: null,
        column: 'col2',
        database: 'main',
        name: 'col2',
        table: 'test',
        type: 'REAL',
      },
      {
        __proto__: null,
        column: 'col3',
        database: 'main',
        name: 'col3',
        table: 'test',
        type: 'TEXT',
      },
      {
        __proto__: null,
        column: 'col4',
        database: 'main',
        name: 'col4',
        table: 'test',
        type: 'BLOB',
      },
      {
        __proto__: null,
        column: 'col5',
        database: 'main',
        name: 'col5',
        table: 'test',
        type: null,
      },
    ]);
  });

  test('supports statements using multiple tables', () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`
      CREATE TABLE test1 (value1 INTEGER);
      CREATE TABLE test2 (value2 INTEGER);
    `);
    const stmt = db.prepare('SELECT value1, value2 FROM test1, test2');
    assert.deepStrictEqual(stmt.columns(), [
      {
        __proto__: null,
        column: 'value1',
        database: 'main',
        name: 'value1',
        table: 'test1',
        type: 'INTEGER',
      },
      {
        __proto__: null,
        column: 'value2',
        database: 'main',
        name: 'value2',
        table: 'test2',
        type: 'INTEGER',
      },
    ]);
  });

  test('supports column aliases', () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`CREATE TABLE test (value INTEGER)`);
    const stmt = db.prepare('SELECT value AS foo FROM test');
    assert.deepStrictEqual(stmt.columns(), [
      {
        __proto__: null,
        column: 'value',
        database: 'main',
        name: 'foo',
        table: 'test',
        type: 'INTEGER',
      },
    ]);
  });

  test('supports column expressions', () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`CREATE TABLE test (value INTEGER)`);
    const stmt = db.prepare('SELECT value + 1, value FROM test');
    assert.deepStrictEqual(stmt.columns(), [
      {
        __proto__: null,
        column: null,
        database: null,
        name: 'value + 1',
        table: null,
        type: null,
      },
      {
        __proto__: null,
        column: 'value',
        database: 'main',
        name: 'value',
        table: 'test',
        type: 'INTEGER',
      },
    ]);
  });

  test('supports subqueries', () => {
    const db = new DatabaseSync(':memory:');
    db.exec(`CREATE TABLE test (value INTEGER)`);
    const stmt = db.prepare('SELECT * FROM (SELECT * FROM test)');
    assert.deepStrictEqual(stmt.columns(), [
      {
        __proto__: null,
        column: 'value',
        database: 'main',
        name: 'value',
        table: 'test',
        type: 'INTEGER',
      },
    ]);
  });

  test('supports statements that do not return data', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE test (value INTEGER)');
    const stmt = db.prepare('INSERT INTO test (value) VALUES (?)');
    assert.deepStrictEqual(stmt.columns(), []);
  });

  test('throws if the statement is finalized', () => {
    const db = new DatabaseSync(':memory:');
    db.exec('CREATE TABLE test (value INTEGER)');
    const stmt = db.prepare('SELECT value FROM test');
    db.close();
    assert.throws(() => {
      stmt.columns();
    }, /statement has been finalized/);
  });
});
