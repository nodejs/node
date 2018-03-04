'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = path.join(tmpdir.path, 'nodejs-benchmark-module');

const bench = common.createBenchmark(main, {
  thousands: [50],
  fullPath: ['true', 'false'],
  useCache: ['true', 'false'],
});

function main({ thousands, fullPath, useCache }) {
  tmpdir.refresh();
  try { fs.mkdirSync(benchmarkDirectory); } catch (e) {}

  for (var i = 0; i <= thousands * 1e3; i++) {
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
    measureFull(thousands, useCache === 'true');
  else
    measureDir(thousands, useCache === 'true');

  tmpdir.refresh();
}

function measureFull(thousands, useCache) {
  var i;
  if (useCache) {
    for (i = 0; i <= thousands * 1e3; i++) {
      require(`${benchmarkDirectory}${i}/index.js`);
    }
  }
  bench.start();
  for (i = 0; i <= thousands * 1e3; i++) {
    require(`${benchmarkDirectory}${i}/index.js`);
  }
  bench.end(thousands);
}

function measureDir(thousands, useCache) {
  var i;
  if (useCache) {
    for (i = 0; i <= thousands * 1e3; i++) {
      require(`${benchmarkDirectory}${i}`);
    }
  }
  bench.start();
  for (i = 0; i <= thousands * 1e3; i++) {
    require(`${benchmarkDirectory}${i}`);
  }
  bench.end(thousands);
}
