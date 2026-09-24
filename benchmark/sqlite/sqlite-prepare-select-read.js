'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [20],
  rows: [100, 10000],
  method: ['all', 'iterate'],
  access: ['named', 'keyed'],
});

const keys = ['text_column', 'integer_column', 'real_column', 'other_column'];

function readNamed(row) {
  return row.text_column.length + row.integer_column + row.real_column +
         row.other_column;
}

function readKeyed(row) {
  let sum = 0;
  for (let k = 0; k < keys.length; k++) sum += row[keys[k]] === null ? 0 : 1;
  return sum;
}

function main(conf) {
  const db = new sqlite.DatabaseSync(':memory:');
  db.exec(
    'CREATE TABLE foo (text_column TEXT, integer_column INTEGER, ' +
    'real_column REAL, blob_column BLOB, other_column INTEGER)',
  );
  const insert = db.prepare('INSERT INTO foo VALUES (?, ?, ?, ?, ?)');
  db.exec('BEGIN');
  for (let i = 0; i < conf.rows; i++) {
    insert.run(`text ${i}`, i, i / 3, Buffer.from('blob'), i * 2);
  }
  db.exec('COMMIT');

  const stmt = db.prepare('SELECT * FROM foo');
  const read = conf.access === 'named' ? readNamed : readKeyed;
  const n = conf.n * (1e5 / conf.rows);
  let sink = 0;

  bench.start();
  for (let i = 0; i < n; i++) {
    for (const row of conf.method === 'all' ? stmt.all() : stmt.iterate()) {
      sink += read(row);
    }
  }
  bench.end(n * conf.rows);

  assert.ok(sink > 0);
}
