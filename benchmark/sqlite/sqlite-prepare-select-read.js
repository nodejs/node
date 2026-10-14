'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e4],
  rows: [1, 100],
  method: ['all', 'iterate', 'get'],
  returnArrays: [0, 1],
});

function main(conf) {
  const db = new sqlite.Database(':memory:');
  db.exec(
    'CREATE TABLE foo (text_column TEXT, integer_column INTEGER, real_column REAL, blob_column BLOB)',
  );
  const insert = db.prepare('INSERT INTO foo VALUES (?, ?, ?, ?)');
  for (let i = 0; i < conf.rows; i++) {
    insert.run(`text ${i}`, i, i / 3, Buffer.from('example blob data'));
  }

  const stmt = db.prepare(`SELECT * FROM foo LIMIT ${conf.rows}`);
  stmt.setReturnArrays(conf.returnArrays === 1);

  // Each row is built and then has every column read once.
  const read = conf.returnArrays ?
    (row) => row[0].length + row[1] + row[2] + row[3].length :
    (row) => row.text_column.length + row.integer_column +
             row.real_column + row.blob_column.length;

  let sum = 0;
  bench.start();
  for (let i = 0; i < conf.n; i++) {
    if (conf.method === 'get') {
      sum += read(stmt.get());
    } else {
      for (const row of stmt[conf.method]()) sum += read(row);
    }
  }
  bench.end(conf.n);

  assert.ok(sum > 0);
}
