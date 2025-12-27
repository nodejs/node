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
    argument: ['true', 'falsePrimitive', 'falseObject'],
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

  const testArgs = [args[argument], args[argument], args[argument]];
  let sum = 0;

  bench.start();
  for (let iteration = 0; iteration < n; iteration++) {
    const testArg = testArgs[iteration % 3];
    sum += func(testArg) ? 1 : 0;
  }
  bench.end(n);
  if (sum < 0) console.log(sum);
}
