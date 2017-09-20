'use strict';
const common = require('../common.js');
const url = require('url');

const inputs = {
  slashes: { slashes: true, host: 'localhost' },
  file: { protocol: 'file:', pathname: '/foo' },
};

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [25e6]
});

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;

  const input = inputs[type] || '';

  // Force-optimize url.format() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (const name in inputs)
    url.format(inputs[name]);

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.format(input);
  bench.end(n);
}
