'use strict';

const common = require('../common.js');
const ClientRequest = require('http').ClientRequest;

const bench = common.createBenchmark(main, {
  len: [1, 8, 16, 32, 64, 128],
  n: [1e6]
});

function main({ len, n }) {
  const path = '/'.repeat(len);
  const opts = { path: path, createConnection: function() {} };

  bench.start();
  for (var i = 0; i < n; i++) {
    new ClientRequest(opts);
  }
  bench.end(n);
}
