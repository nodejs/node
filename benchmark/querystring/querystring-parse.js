'use strict';
var common = require('../common.js');
var querystring = require('querystring');
var inputs = require('../fixtures/url-inputs.js').searchParams;

var bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e6],
});

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;
  var input = inputs[type];
  var i;

  // Optimize querystring.parse() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  if (type !== 'multicharsep') {
    for (i = 0; i < n; i += 1)
      querystring.parse(input);
  } else {
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');
  }

  if (type !== 'multicharsep') {
    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input);
    bench.end(n);
  } else {
    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');
    bench.end(n);
  }
}
