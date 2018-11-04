'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = path.join(tmpdir.path, 'nodejs-benchmark-module');

const bench = common.createBenchmark(main, {
  n: [5e4],
  fullPath: ['true', 'false'],
  useCache: ['true', 'false']
});

function main({ n, fullPath, useCache }) {
  tmpdir.refresh();
  try { fs.mkdirSync(benchmarkDirectory); } catch {}
  for (var i = 0; i <= n; i++) {
    fs.mkdirSync(`${benchmarkDirectory}${i}`);
    fs.writeFileSync(
      `${benchmarkDirectory}${i}/package.json`,
      '{"main": "index.js"}'
    );
    fs.writeFileSync(
      `${benchmarkDirectory}${i}/index.js`,
      'module.exports = "";'
    );
  }

  if (fullPath === 'true')
    measureFull(n, useCache === 'true');
  else
    measureDir(n, useCache === 'true');

  tmpdir.refresh();
}

function measureFull(n, useCache) {
  var i;
  if (useCache) {
    for (i = 0; i <= n; i++) {
      require(`${benchmarkDirectory}${i}/index.js`);
    }
  }
  bench.start();
  for (i = 0; i <= n; i++) {
    require(`${benchmarkDirectory}${i}/index.js`);
  }
  bench.end(n);
}

function measureDir(n, useCache) {
  var i;
  if (useCache) {
    for (i = 0; i <= n; i++) {
      require(`${benchmarkDirectory}${i}`);
    }
  }
  bench.start();
  for (i = 0; i <= n; i++) {
    require(`${benchmarkDirectory}${i}`);
  }
  bench.end(n);
}
