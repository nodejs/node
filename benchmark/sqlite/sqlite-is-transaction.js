'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  transaction: ['true', 'false'],
});

function main(conf) {
  const db = new sqlite.DatabaseSync(':memory:');

  if (conf.transaction === 'true') {
    db.exec('BEGIN');
  }

  let i;
  let deadCodeElimination = true;

  bench.start();
  for (i = 0; i < conf.n; i += 1)
    deadCodeElimination &&= db.isTransaction;
  bench.end(conf.n);

  assert.ok(deadCodeElimination === (conf.transaction === 'true'));

  if (conf.transaction === 'true') {
    db.exec('ROLLBACK');
  }
}
