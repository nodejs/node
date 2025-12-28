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

  const testArgs = [args.true, args.falsePrimitive, args.falseObject];

  bench.start();
  const start = performance.now();
  for (let iteration = 0; iteration < n; iteration++) {
    const testArg = testArgs[iteration % testArgs.length];
    func(testArg);
  }
  const end = performance.now();
  bench.end(n);
  console.log('Elapsed(ms):', (end - start).toFixed(3));
}
