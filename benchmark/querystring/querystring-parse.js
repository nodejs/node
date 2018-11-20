var common = require('../common.js');
var querystring = require('querystring');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: ['noencode', 'encodemany', 'encodelast'],
  n: [1e6],
});

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;

  var inputs = {
    noencode: 'foo=bar&baz=quux&xyzzy=thud',
    encodemany: '%66%6F%6F=bar&%62%61%7A=quux&xyzzy=%74h%75d',
    encodelast: 'foo=bar&baz=quux&xyzzy=thu%64'
  };
  var input = inputs[type];

  // Force-optimize querystring.parse() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (var name in inputs)
    querystring.parse(inputs[name]);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(querystring.parse)');

  bench.start();
  for (var i = 0; i < n; i += 1)
    querystring.parse(input);
  bench.end(n);
}
