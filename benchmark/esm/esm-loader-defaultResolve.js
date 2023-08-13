// Tests the impact on eager operations required for policies affecting
// general startup, does not test lazy operations
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir.js');
const { pathToFileURL } = require('node:url');

const benchmarkDirectory = tmpdir.resolve('benchmark-import-meta-resolve');

const parentURL = pathToFileURL(path.join(benchmarkDirectory, 'entry-point.js'));

const configs = {
  n: [1e3],
  specifier: [
    './relative-existing.js',
    './relative-nonexistent.js',
    'unprefixed-existing',
    'unprefixed-nonexistent',
    'node:prefixed-nonexistent',
    'node:os',
  ],
};

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(main, configs, options);

function main(conf) {
  const { defaultResolve } = require('internal/modules/esm/resolve');
  tmpdir.refresh();

  fs.mkdirSync(path.join(benchmarkDirectory, 'node_modules', 'unprefixed-existing'), { recursive: true });
  fs.writeFileSync(path.join(benchmarkDirectory, 'node_modules', 'unprefixed-existing', 'index.js'), '\n');
  fs.writeFileSync(path.join(benchmarkDirectory, 'relative-existing.js'), '\n');

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    try {
      defaultResolve(conf.specifier, { parentURL });
    } catch { /* empty */ }
  }

  bench.end(conf.n);
}
