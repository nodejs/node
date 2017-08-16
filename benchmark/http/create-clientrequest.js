'use strict';

var common = require('../common.js');
var ClientRequest = require('http').ClientRequest;

var bench = common.createBenchmark(main, {
  len: [1, 8, 16, 32, 64, 128],
  n: [1e6]
});

function main(conf) {
  var len = +conf.len;
  var n = +conf.n;

  var path = '/'.repeat(len);
  var opts = { path: path, createConnection: function() {} };

  bench.start();
  for (var i = 0; i < n; i++) {
    new ClientRequest(opts);
  }
  bench.end(n);
}
