'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = tmpdir.resolve('benchmark-module-circular');

const bench = common.createBenchmark(main, {
  n: [1e4],
});

function main({ n }) {
  tmpdir.refresh();

  const aDotJS = path.join(benchmarkDirectory, 'a.js');
  const bDotJS = path.join(benchmarkDirectory, 'b.js');

  fs.mkdirSync(benchmarkDirectory);
  fs.writeFileSync(aDotJS, 'require("./b.js");');
  fs.writeFileSync(bDotJS, 'require("./a.js");');

  bench.start();
  for (let i = 0; i < n; i++) {
    require(aDotJS);
    require(bDotJS);
    delete require.cache[aDotJS];
    delete require.cache[bDotJS];
  }
  bench.end(n);

  tmpdir.refresh();
}
