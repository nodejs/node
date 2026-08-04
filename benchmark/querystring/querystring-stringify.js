'use strict';
const common = require('../common.js');
const querystring = require('querystring');

const bench = common.createBenchmark(main, {
  type: ['noencode', 'encodemany', 'encodelast', 'array', 'multiprimitives'],
  n: [1e6],
});

function main({ type, n }) {
  const inputs = {
    noencode: {
      foo: 'bar',
      baz: 'quux',
      xyzzy: 'thud',
    },
    encodemany: {
      '\u0080\u0083\u0089': 'bar',
      '\u008C\u008E\u0099': 'quux',
      'xyzzy': '\u00A5q\u00A3r',
    },
    encodelast: {
      foo: 'bar',
      baz: 'quux',
      xyzzy: 'thu\u00AC',
    },
    array: {
      foo: [],
      baz: ['bar'],
      xyzzy: ['bar', 'quux', 'thud'],
    },
    multiprimitives: {
      foo: false,
      bar: -13.37,
      baz: '',
    },
  };
  const input = inputs[type];

  // Force-optimize querystring.stringify() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (const name in inputs)
    querystring.stringify(inputs[name]);

  bench.start();
  for (let i = 0; i < n; i += 1)
    querystring.stringify(input);
  bench.end(n);
}
