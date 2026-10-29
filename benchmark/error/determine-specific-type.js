'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e6],
  v: [
    '() => 1n',
    '() => true',
    '() => false',
    '() => 2',
    '() => +0',
    '() => -0',
    '() => NaN',
    '() => Infinity',
    '() => ""',
    '() => "\'"',
    '() => Symbol("foo")',
    '() => function foo() {}',
    '() => null',
    '() => undefined',
    '() => new Array()',
    '() => new BigInt64Array()',
    '() => new BigUint64Array()',
    '() => new Int8Array()',
    '() => new Int16Array()',
    '() => new Int32Array()',
    '() => new Float32Array()',
    '() => new Float64Array()',
    '() => new Uint8Array()',
    '() => new Uint8ClampedArray()',
    '() => new Uint16Array()',
    '() => new Uint32Array()',
    '() => new Date()',
    '() => new Map()',
    '() => new WeakMap()',
    '() => new Object()',
    '() => Promise.resolve("foo")',
    '() => new Set()',
    '() => new WeakSet()',
    '() => ({ __proto__: null })',
  ],
}, {
  flags: ['--expose-internals'],
});

function main({ n, v }) {
  const {
    determineSpecificType,
  } = require('internal/errors');

  const value = eval(v)();

  bench.start();
  for (let i = 0; i < n; ++i)
    determineSpecificType(value);
  bench.end(n);
}
