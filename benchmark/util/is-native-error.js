'use strict';

const common = require('../common');

const args = {
  true: new Error('test'),
  falsePrimitive: 42,
  falseObject: { foo: 'bar' },
};

const bench = common.createBenchmark(
  main,
  {
    argument: ['true'],
    version: ['native', 'js'],
    n: [1e6],
  },
  {
    flags: ['--expose-internals', '--no-warnings'],
  },
);

function main({ argument, version, n }) {
  const util = common.binding('util');
  const types = require('internal/util/types');

  const func = { native: util, js: types }[version].isNativeError;
  const arg = args[argument];

  bench.start();
  let result;
  for (let iteration = 0; iteration < n; iteration++) {
    result = func(arg);
  }
  bench.end(n);
  if (typeof result !== 'boolean') throw new Error('unexpected result');
}
