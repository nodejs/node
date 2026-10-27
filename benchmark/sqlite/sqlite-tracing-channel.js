'use strict';
const common = require('../common.js');
const sqlite = require('node:sqlite');
const dc = require('node:diagnostics_channel');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  query: ['get', 'run', 'run-error'],
  mode: ['none', 'subscribed', 'unsubscribed'],
});

function main(conf) {
  const { n, query, mode } = conf;

  const db = new sqlite.Database(':memory:');
  db.exec('CREATE TABLE t (x INTEGER PRIMARY KEY)');
  db.exec('INSERT INTO t VALUES (1)');
  let stmt;
  let method = 'run';
  if (query === 'get') {
    stmt = db.prepare('SELECT x FROM t WHERE x = ?');
    method = 'get';
  } else if (query === 'run') {
    stmt = db.prepare('INSERT OR REPLACE INTO t VALUES (?)');
  } else {
    // Always violates the primary key, so every call throws.
    stmt = db.prepare('INSERT INTO t VALUES (?)');
  }

  const channel = dc.tracingChannel('sqlite.query');
  const handlers = { start() {}, end() {}, error() {} };
  if (mode === 'subscribed') {
    channel.subscribe(handlers);
  } else if (mode === 'unsubscribed') {
    channel.subscribe(handlers);
    channel.unsubscribe(handlers);
  }
  // mode === 'none': no subscription ever made

  let result;
  let errors = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      result = stmt[method](1);
    } catch {
      errors++;
    }
  }
  bench.end(n);

  if (mode === 'subscribed') {
    channel.unsubscribe(handlers);
  }

  if (query === 'run-error') {
    assert.strictEqual(errors, n);
  } else {
    assert.ok(result !== undefined);
  }
}
