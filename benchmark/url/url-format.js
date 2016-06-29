'use strict';
const common = require('../common.js');
const url = require('url');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  type: 'one two'.split(' '),
  n: [25e6]
});

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;

  const inputs = {
    one: {slashes: true, host: 'localhost'},
    two: {protocol: 'file:', pathname: '/foo'},
  };
  const input = inputs[type] || '';

  // Force-optimize url.format() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (const name in inputs)
    url.format(inputs[name]);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(url.format)');

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.format(input);
  bench.end(n);
}
