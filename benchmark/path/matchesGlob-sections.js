'use strict';

const common = require('../common');
const assert = require('node:assert');
const { posix, win32 } = require('node:path');

const bench = common.createBenchmark(main, {
  n: [1e5],
  platform: ['posix', 'win32'],
  count: [1, 2, 4, 10, 16],
  result: ['hit', 'miss'],
});

function main({ n, platform, count, result }) {
  const implementation = platform === 'posix' ? posix : win32;
  const names = Array.from({ length: count - 1 }, (_, index) => `part${index}`);
  const pattern = ['**', ...names.flatMap((name) => [name, '**']), 'file.js'].join('/');
  const parts = ['root', ...names.flatMap((name) => [name, 'dir']), 'file.js'];
  if (result === 'miss') {
    if (names.length) parts[parts.length - 3] = 'absent';
    else parts[parts.length - 1] = 'file.txt';
  }
  const filename = parts.join(implementation.sep);
  const expected = result === 'hit';
  assert.strictEqual(implementation.matchesGlob(filename, pattern), expected);
  let actual;
  bench.start();
  for (let index = 0; index < n; index++) actual = implementation.matchesGlob(filename, pattern);
  bench.end(n);
  assert.strictEqual(actual, expected);
}
