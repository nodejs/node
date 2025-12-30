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
    version: ['instanceof', 'isError'],
    n: [1e6],
  },
);

function main({ argument, version, n }) {
  const arg = args[argument];
  const func = version === 'isError' ? Error.isError : (v) => v instanceof Error;

  bench.start();
  let result;
  for (let i = 0; i < n; i++) {
    result = func(arg);
  }
  bench.end(n);
  if (typeof result !== 'boolean') throw new Error('unexpected result');
}
