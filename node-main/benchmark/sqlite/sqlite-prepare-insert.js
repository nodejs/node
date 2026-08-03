'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  statement: [
    'INSERT INTO text_column_type (text_column) VALUES (?)',
    'INSERT INTO integer_column_type (integer_column) VALUES (?)',
    'INSERT INTO real_column_type (real_column) VALUES (?)',
    'INSERT INTO blob_column_type (blob_column) VALUES (?)',
    'INSERT INTO all_column_types (text_column, integer_column, real_column, blob_column) VALUES (?, ?, ?, ?)',
    'INSERT INTO large_text (text_8kb_column) VALUES (?)',
    'INSERT INTO missing_required_value (any_value, required_value) VALUES (?, ?)',
  ],
});

function main(conf) {
  const db = new sqlite.DatabaseSync(':memory:');

  db.exec('CREATE TABLE text_column_type (text_column TEXT)');
  db.exec('CREATE TABLE integer_column_type (integer_column INTEGER)');
  db.exec('CREATE TABLE real_column_type (real_column REAL)');
  db.exec('CREATE TABLE blob_column_type (blob_column BLOB)');
  db.exec(
    'CREATE TABLE all_column_types (text_column TEXT, integer_column INTEGER, real_column REAL, blob_column BLOB)',
  );
  db.exec('CREATE TABLE large_text (text_8kb_column TEXT)');
  db.exec('CREATE TABLE missing_required_value (any_value TEXT, required_value TEXT NOT NULL)');

  const insertStatement = db.prepare(conf.statement);

  let i;
  let deadCodeElimination;

  const stringValue = crypto.randomUUID();
  const integerValue = Math.floor(Math.random() * 100);
  const realValue = Math.random();
  const blobValue = Buffer.from('example blob data');

  const largeText = 'a'.repeat(8 * 1024);

  if (conf.statement.includes('text_column_type')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      deadCodeElimination = insertStatement.run(stringValue);
    }
    bench.end(conf.n);
  } else if (conf.statement.includes('integer_column_type')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      deadCodeElimination = insertStatement.run(integerValue);
    }
    bench.end(conf.n);
  } else if (conf.statement.includes('real_column_type')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      deadCodeElimination = insertStatement.run(realValue);
    }
    bench.end(conf.n);
  } else if (conf.statement.includes('blob_column_type')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      deadCodeElimination = insertStatement.run(blobValue);
    }
    bench.end(conf.n);
  } else if (conf.statement.includes('INTO all_column_types')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      deadCodeElimination = insertStatement.run(
        stringValue,
        integerValue,
        realValue,
        blobValue,
      );
    }
    bench.end(conf.n);
  } else if (conf.statement.includes('INTO large_text')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      deadCodeElimination = insertStatement.run(largeText);
    }
    bench.end(conf.n);
  } else if (conf.statement.includes('missing_required_value')) {
    bench.start();
    for (i = 0; i < conf.n; i += 1) {
      try {
        deadCodeElimination = insertStatement.run(stringValue);
      } catch (e) {
        deadCodeElimination = e;
      }
    }
    bench.end(conf.n);
  } else {
    throw new Error('Unknown statement');
  }

  assert.ok(deadCodeElimination !== undefined);
}
