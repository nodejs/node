'use strict';

// require() of a package that is already loaded, from a directory other than
// the one that first loaded it, with and without package.json "exports".

const fs = require('fs');
const path = require('path');
const { createRequire } = require('module');
const common = require('../common.js');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  exports: ['true', 'false'],
  n: [1e5],
});

function main({ exports, n }) {
  tmpdir.refresh();
  const root = tmpdir.resolve('nodejs-benchmark-module-exports');
  const pkgDir = path.join(root, 'node_modules', 'pkg');
  const consumerDir = path.join(root, 'node_modules', 'consumer');
  fs.mkdirSync(pkgDir, { recursive: true });
  fs.mkdirSync(consumerDir);

  const manifest = { name: 'pkg', main: 'index.js' };
  if (exports === 'true') {
    manifest.exports = './index.js';
  }
  fs.writeFileSync(path.join(pkgDir, 'package.json'), JSON.stringify(manifest));
  fs.writeFileSync(path.join(pkgDir, 'index.js'), 'module.exports = {};');
  fs.writeFileSync(path.join(consumerDir, 'index.js'),
                   'module.exports = () => require("pkg");');

  const rootRequire = createRequire(path.join(root, 'index.js'));
  rootRequire('pkg');
  const load = rootRequire('consumer');

  bench.start();
  for (let i = 0; i < n; i++) {
    load();
  }
  bench.end(n);
}
