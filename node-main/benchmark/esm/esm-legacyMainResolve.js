// Tests the impact on eager operations required for policies affecting
// general startup, does not test lazy operations
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir.js');
const { pathToFileURL } = require('node:url');

const benchmarkDirectory = tmpdir.resolve('benchmark-import-meta-resolve');

const configs = {
  n: [1e4],
  packageJsonUrl: [
    'node_modules/test/package.json',
  ],
  packageConfigMain: ['', './index.js'],
  resolvedFile: [
    'node_modules/test/index.js',
    'node_modules/test/index.json',
    'node_modules/test/index.node',
    'node_modules/non-exist',
  ],
};

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(main, configs, options);

function main(conf) {
  const { legacyMainResolve } = require('internal/modules/esm/resolve');
  tmpdir.refresh();

  fs.mkdirSync(path.join(benchmarkDirectory, 'node_modules', 'test'), { recursive: true });
  fs.writeFileSync(path.join(benchmarkDirectory, conf.resolvedFile), '\n');

  const packageJsonUrl = pathToFileURL(conf.packageJsonUrl);
  const packageConfigMain = { main: conf.packageConfigMain };

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    try {
      legacyMainResolve(packageJsonUrl, packageConfigMain, undefined);
    } catch { /* empty */ }
  }

  bench.end(conf.n);
}
