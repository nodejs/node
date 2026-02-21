'use strict';
const common = require('../common.js');
const dc = require('diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e5],
});

function noop() { }

function main({ n }) {

  bench.start();
  for (let i = 0; i < n; i++) {
    dc.subscribe('channel.0', noop);
  }
  bench.end(n);
}
