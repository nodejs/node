'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();
let dirc = 0;

const bench = common.createBenchmark(main, {
  n: [1e4],
});

function main({ n }) {
  bench.start();
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    const pathname = `${tmpdir.path}/${++dirc}/${++dirc}/${++dirc}/${++dirc}`;
    fs.mkdir(pathname, { recursive: true }, (err) => {
      r(cntr);
    });
  }(n));
}
