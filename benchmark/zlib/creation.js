'use strict';
var common = require('../common.js');
var zlib = require('zlib');

var bench = common.createBenchmark(main, {
  type: [
    'Deflate', 'DeflateRaw', 'Inflate', 'InflateRaw', 'Gzip', 'Gunzip', 'Unzip'
  ],
  options: ['true', 'false'],
  n: [5e5]
});

function main(conf) {
  var n = +conf.n;
  var fn = zlib['create' + conf.type];
  if (typeof fn !== 'function')
    throw new Error('Invalid zlib type');
  var i = 0;

  if (conf.options === 'true') {
    var opts = {};
    bench.start();
    for (; i < n; ++i)
      fn(opts);
    bench.end(n);
  } else {
    bench.start();
    for (; i < n; ++i)
      fn();
    bench.end(n);
  }
}
