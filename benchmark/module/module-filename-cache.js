'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = path.join(
  tmpdir.path,
  'nodejs-benchmark-module-filename-cache'
);

const bench = common.createBenchmark(main, {
  requireCount: [1e4, 1e5, 1e6, 1e7]
});

function main({ requireCount }) {
  tmpdir.refresh();
  try { fs.mkdirSync(benchmarkDirectory); } catch {}

  fs.mkdirSync(`${benchmarkDirectory}${requireCount}`);
  const filename = `${benchmarkDirectory}${requireCount}/index.js`;
  fs.writeFileSync(
    filename,
    'module.exports = "";'
  );

  bench.start();
  for (let i = 0; i <= requireCount; i++) {
    require(filename);
  }
  bench.end(requireCount);

  tmpdir.refresh();
}
