'use strict';
const common = require('../common.js');
const dc = require('diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e8],
});

function noop() {}

function main({ n }) {
  const channel = dc.channel('channel.0');

  bench.start();
  for (let i = 0; i < n; i++) {
    channel.subscribe(noop);
  }
  bench.end(n);
}
