'use strict';
var common = require('../common.js');
var querystring = require('querystring');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: ['noencode', 'encodemany', 'encodelast'],
  n: [1e7],
});

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;

  var inputs = {
    noencode: {
      foo: 'bar',
      baz: 'quux',
      xyzzy: 'thud'
    },
    encodemany: {
      '\u0080\u0083\u0089': 'bar',
      '\u008C\u008E\u0099': 'quux',
      xyzzy: '\u00A5q\u00A3r'
    },
    encodelast: {
      foo: 'bar',
      baz: 'quux',
      xyzzy: 'thu\u00AC'
    }
  };
  var input = inputs[type];

  // Force-optimize querystring.stringify() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (var name in inputs)
    querystring.stringify(inputs[name]);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(querystring.stringify)');
  querystring.stringify(input);

  bench.start();
  for (var i = 0; i < n; i += 1)
    querystring.stringify(input);
  bench.end(n);
}
