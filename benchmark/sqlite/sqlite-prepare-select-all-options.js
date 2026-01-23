'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  tableSeedSize: [1e5],
  statement: [
    'SELECT * FROM foo LIMIT 1',
    'SELECT * FROM foo LIMIT 100',
  ],
  options: ['none', 'readBigInts', 'returnArrays', 'readBigInts|returnArrays'],
});

function main(conf) {
  const optionsObj = conf.options === 'none' ? {} : conf.options.split('|').reduce((acc, key) => {
    acc[key] = true;
    return acc;
  }, {});

  const db = new sqlite.DatabaseSync(':memory:', optionsObj);

  db.exec(
    'CREATE TABLE foo (text_column TEXT, integer_column INTEGER, real_column REAL, blob_column BLOB)',
  );

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

  let i;
  let deadCodeElimination;

  const stmt = db.prepare(conf.statement);

  bench.start();
  for (i = 0; i < conf.n; i += 1) deadCodeElimination = stmt.all();
  bench.end(conf.n);

  assert.ok(deadCodeElimination !== undefined);
}
