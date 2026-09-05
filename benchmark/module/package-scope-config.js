'use strict';

const common = require('../common');
const fs = require('fs');
const { pathToFileURL } = require('url');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  entries: [0, 20, 200, 1000],
  n: [1e4],
}, { flags: ['--expose-internals'] });

function main({ entries, n }) {
  const { getPackageScopeConfig } = require('internal/modules/package_json_reader');
  tmpdir.refresh();
  const imports = {};
  const exports = {};
  for (let i = 0; i < entries; i++) {
    imports[`#entry${i}`] = `./entry${i}.js`;
    exports[`./entry${i}`] = `./entry${i}.js`;
  }
  fs.writeFileSync(tmpdir.resolve('package.json'), JSON.stringify({
    type: 'module', imports, exports,
  }));
  const urls = Array.from({ length: 100 }, (_, i) =>
    pathToFileURL(tmpdir.resolve(`entry${i}.js`)).href);
  // Warm the package cache while looking up distinct modules in one scope.
  for (const url of urls) getPackageScopeConfig(url);

  bench.start();
  for (let i = 0; i < n; i++) {
    getPackageScopeConfig(urls[i % urls.length]);
  }
  bench.end(n);
}
