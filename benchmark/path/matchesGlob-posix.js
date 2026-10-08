'use strict';
const common = require('../common.js');
const { posix } = require('path');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  path: [
    'src/index.ts',
    'src/index.test.ts',
    'src/foo/bar/biz/baz/index.test.ts',
  ],
  pattern: [
    'src/index.ts',
    '**/*.ts',
    '**/*.{ts,js}',
    'src/**/baz/*.test.ts',
    'src/**/baz/*.ts',
    'src/**/index.@(test|spec).ts',
    'test/**/*.ts',
  ],
  n: [1e5],
});

function main({ path, pattern, n }) {
  bench.start();
  let a;
  for (let i = 0; i < n; i++) {
    a = posix.matchesGlob(path, pattern);
  }
  bench.end(n);
  assert.strictEqual(typeof a, 'boolean');
}
