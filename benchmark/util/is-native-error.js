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
    version: ['native'],
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
  for (let iteration = 0; iteration < n; iteration++) {
    func(arg);
  }
  bench.end(n);
}
