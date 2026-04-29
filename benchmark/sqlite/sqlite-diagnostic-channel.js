'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const dc = require('diagnostics_channel');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  mode: ['none', 'subscribed', 'unsubscribed'],
});

function main(conf) {
  const { n, mode } = conf;

  const db = new sqlite.DatabaseSync(':memory:');
  db.exec('CREATE TABLE t (x INTEGER)');
  const insert = db.prepare('INSERT INTO t VALUES (?)');

  let subscriber;
  if (mode === 'subscribed') {
    subscriber = () => {};
    dc.subscribe('sqlite.db.query', subscriber);
  } else if (mode === 'unsubscribed') {
    subscriber = () => {};
    dc.subscribe('sqlite.db.query', subscriber);
    dc.unsubscribe('sqlite.db.query', subscriber);
  }
  // mode === 'none': no subscription ever made

  let result;
  bench.start();
  for (let i = 0; i < n; i++) {
    result = insert.run(i);
  }
  bench.end(n);

  if (mode === 'subscribed') {
    dc.unsubscribe('sqlite.db.query', subscriber);
  }

  assert.ok(result !== undefined);
}
