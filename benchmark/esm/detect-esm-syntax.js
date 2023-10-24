'use strict';

// This benchmarks the cost of running `containsModuleSyntax` on a CommonJS module being imported.
// We use the TypeScript fixture because it's a very large CommonJS file with no ESM syntax: the worst case.
const common = require('../common.js');
const tmpdir = require('../../test/common/tmpdir.js');
const fixtures = require('../../test/common/fixtures.js');
const scriptPath = fixtures.path('snapshot', 'typescript.js');
const fs = require('node:fs');

const bench = common.createBenchmark(main, {
  type: ['with-module-syntax-detection', 'without-module-syntax-detection'],
  n: [1e4],
}, {
  flags: ['--experimental-detect-module'],
});

const benchmarkDirectory = tmpdir.fileURL('benchmark-detect-esm-syntax');
const ambiguousURL = new URL('./typescript.js', benchmarkDirectory);
const explicitURL = new URL('./typescript.cjs', benchmarkDirectory);

async function main({ n, type }) {
  tmpdir.refresh();

  fs.mkdirSync(benchmarkDirectory, { recursive: true });
  fs.cpSync(scriptPath, ambiguousURL);
  fs.cpSync(scriptPath, explicitURL);

  bench.start();

  for (let i = 0; i < n; i++) {
    const url = type === 'with-module-syntax-detection' ? ambiguousURL : explicitURL;
    await import(url);
  }

  bench.end(n);
}
