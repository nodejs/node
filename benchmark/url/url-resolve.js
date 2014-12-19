var common = require('../common.js');
var url = require('url');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: ['one'],
  n: [1e5],
});

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;

  var inputs = {
    one: ['http://example.com/', '../../../../../etc/passwd'],
  };
  var input = inputs[type] || [];

  // Force-optimize url.resolve() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (var name in inputs)
    url.resolve(inputs[name][0], inputs[name][1]);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(url.resolve)');

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.resolve(input[0], input[1]);
  bench.end(n);
}
