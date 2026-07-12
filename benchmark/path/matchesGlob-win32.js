'use strict';
const common = require('../common.js');
const { win32 } = require('path');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  path: [
    'src/index.ts',
    'src\\index.ts',
    'src/foo/bar/biz/baz/index.test.ts',
    'src\\foo\\bar\\biz\\baz\\index.test.ts',
  ],
  pattern: [
    'src/**/baz/*.test.ts',
    'src/**/baz/*.ts',
    'test/**/*.ts',
  ],
  n: [1e5],
});

function main({ path, pattern, n }) {
  bench.start();
  let a;
  for (let i = 0; i < n; i++) {
    a = win32.matchesGlob(path, pattern);
  }
  bench.end(n);
  assert(a + 'a');
}
