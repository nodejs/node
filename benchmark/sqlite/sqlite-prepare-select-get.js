'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  tableSeedSize: [1e5],
  statement: [
    'SELECT 1',
    'SELECT * FROM foo LIMIT 1',
    'SELECT text_column FROM foo LIMIT 1',
    'SELECT text_column, integer_column FROM foo LIMIT 1',
    'SELECT text_column, integer_column, real_column FROM foo LIMIT 1',
    'SELECT text_column, integer_column, real_column, blob_column FROM foo LIMIT 1',
    'SELECT text_8kb_column FROM foo_large LIMIT 1',
  ],
});

function main(conf) {
  const db = new sqlite.DatabaseSync(':memory:');

  db.exec('CREATE TABLE foo (text_column TEXT, integer_column INTEGER, real_column REAL, blob_column BLOB)');
  const fooInsertStatement = db.prepare(
    'INSERT INTO foo (text_column, integer_column, real_column, blob_column) VALUES (?, ?, ?, ?)',
  );

  for (let i = 0; i < conf.tableSeedSize; i++) {
    fooInsertStatement.run(
      crypto.randomUUID(),
      Math.floor(Math.random() * 100),
      Math.random(),
      Buffer.from('example blob data'),
    );
  }

  db.exec('CREATE TABLE foo_large (text_8kb_column TEXT)');
  const fooLargeInsertStatement = db.prepare('INSERT INTO foo_large (text_8kb_column) VALUES (?)');
  const largeText = 'a'.repeat(8 * 1024);
  for (let i = 0; i < conf.tableSeedSize; i++) {
    fooLargeInsertStatement.run(largeText);
  }

  let i;
  let deadCodeElimination;

  const stmt = db.prepare(conf.statement);

  bench.start();
  for (i = 0; i < conf.n; i += 1)
    deadCodeElimination = stmt.get();
  bench.end(conf.n);

  assert.ok(deadCodeElimination !== undefined);
}
