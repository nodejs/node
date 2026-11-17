'use strict';

const common = require('../common');
const assert = require('node:assert');
const { posix, win32 } = require('node:path');

const bench = common.createBenchmark(main, {
  n: [2e4],
  platform: ['posix', 'win32'],
  pattern: ['**/node_modules/**/test/**', '**/src/**'],
});

function main({ n, platform, pattern }) {
  const implementation = platform === 'posix' ? posix : win32;
  const filenames = Array.from({ length: 64 }, (_, index) => {
    const directory = index % 2 === 0 ? 'node_modules/dependency/test' : 'src';
    return `workspace/packages/package${index}/${directory}/file${index}.js`
      .replaceAll('/', implementation.sep);
  });
  assert.strictEqual(filenames.filter((filename) => implementation.matchesGlob(filename, pattern)).length, 32);
  let matches = 0;
  bench.start();
  for (let iteration = 0; iteration < n; iteration++) {
    for (const filename of filenames) matches += implementation.matchesGlob(filename, pattern);
  }
  bench.end(n * filenames.length);
  assert.strictEqual(matches, n * 32);
}
