// Tests the impact on eager operations required for policies affecting
// general startup, does not test lazy operations
'use strict';
const fs = require('node:fs');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir.js');

const benchmarkDirectory = tmpdir.fileURL('benchmark-import');

const configs = {
  n: [1e3],
  specifier: [
    'data:text/javascript,{i}',
    './relative-existing.js',
    './relative-nonexistent.js',
    'node:prefixed-nonexistent',
    'node:os',
  ],
};

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(main, configs, options);

async function main(conf) {
  tmpdir.refresh();

  fs.mkdirSync(benchmarkDirectory, { recursive: true });
  fs.writeFileSync(new URL('./relative-existing.js', benchmarkDirectory), '\n');

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    try {
      await import(new URL(conf.specifier.replace('{i}', i), benchmarkDirectory));
    } catch { /* empty */ }
  }

  bench.end(conf.n);
}
