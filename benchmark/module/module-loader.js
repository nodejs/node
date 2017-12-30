'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const { refreshTmpDir, tmpDir } = require('../../test/common');
const benchmarkDirectory = path.join(tmpDir, 'nodejs-benchmark-module');

const bench = common.createBenchmark(main, {
  thousands: [50],
  fullPath: ['true', 'false'],
  useCache: ['true', 'false']
});

function main({ thousands, fullPath, useCache }) {
  const n = thousands * 1e3;

  refreshTmpDir();
  try { fs.mkdirSync(benchmarkDirectory); } catch (e) {}

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

  refreshTmpDir();
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
  bench.end(n / 1e3);
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
  bench.end(n / 1e3);
}
