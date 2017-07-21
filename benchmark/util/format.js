'use strict';

const util = require('util');
const common = require('../common');
const types = [
  'string',
  'number',
  'object',
  'unknown',
  'no-replace'
];
const bench = common.createBenchmark(main, {
  n: [1e6],
  type: types
});

const inputs = {
  'string': ['Hello, my name is %s', 'fred'],
  'number': ['Hi, I was born in %d', 1942],
  'object': ['An error occurred %j', { msg: 'This is an error', code: 'ERR' }],
  'unknown': ['hello %a', 'test'],
  'no-replace': [1, 2]
};

function main(conf) {
  const n = conf.n | 0;
  const type = conf.type;

  const input = inputs[type];

  bench.start();
  for (var i = 0; i < n; i++) {
    util.format(input[0], input[1]);
  }
  bench.end(n);
}
