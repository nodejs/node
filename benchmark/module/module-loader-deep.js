'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = path.join(tmpdir.path, 'nodejs-benchmark-module');

const bench = common.createBenchmark(main, {
  ext: ['', '.js'],
  files: [1e3],
  cache: ['true', 'false']
});

function main({ ext, cache, files }) {
  tmpdir.refresh();
  fs.mkdirSync(benchmarkDirectory);
  fs.writeFileSync(
    `${benchmarkDirectory}/a.js`,
    'module.exports = {};'
  );
  for (let i = 0; i <= files; i++) {
    fs.mkdirSync(`${benchmarkDirectory}/${i}`);
    fs.writeFileSync(
      `${benchmarkDirectory}/${i}/package.json`,
      '{"main": "index.js"}'
    );
    fs.writeFileSync(
      `${benchmarkDirectory}/${i}/index.js`,
      `require('../a${ext}');`
    );
  }

  measureDir(cache === 'true', files);

  tmpdir.refresh();
}

function measureDir(cache, files) {
  if (cache) {
    for (let i = 0; i <= files; i++) {
      require(`${benchmarkDirectory}/${i}`);
    }
  }
  bench.start();
  for (let i = 0; i <= files; i++) {
    require(`${benchmarkDirectory}/${i}`);
  }
  bench.end(files);
}
