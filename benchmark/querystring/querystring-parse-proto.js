'use strict';
var common = require('../common.js');
var querystring = require('querystring');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [1e6],
});

function main(conf) {
  var n = conf.n | 0;

  const input = 'a=b&__proto__=1';

  v8.setFlagsFromString('--allow_natives_syntax');
  querystring.parse(input);
  eval('%OptimizeFunctionOnNextCall(querystring.parse)');
  querystring.parse(input);

  var i;
  bench.start();
  for (i = 0; i < n; i += 1)
    querystring.parse(input);
  bench.end(n);
}
