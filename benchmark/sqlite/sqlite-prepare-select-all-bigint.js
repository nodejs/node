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
    'SELECT * FROM foo LIMIT 100',
    'SELECT integer_column FROM foo LIMIT 1',
    'SELECT integer_column FROM foo LIMIT 100',
  ],
});

function main(conf) {
  const db = new sqlite.DatabaseSync(':memory:', {
    readBigInts: true,
  });

  db.exec('CREATE TABLE foo (integer_column INTEGER)');

  const fooInsertStatement = db.prepare(
    'INSERT INTO foo (integer_column) VALUES (?)',
  );

  for (let i = 0; i < conf.tableSeedSize; i++) {
    fooInsertStatement.run(Math.floor(Math.random() * 100));
  }

  let i;
  let deadCodeElimination;

  const stmt = db.prepare(conf.statement);

  bench.start();
  for (i = 0; i < conf.n; i += 1) deadCodeElimination = stmt.all();
  bench.end(conf.n);

  assert.ok(deadCodeElimination !== undefined);
}
