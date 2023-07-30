'use strict';

const common = require('../common.js');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  encoding: ['undefined', 'utf8'],
  path: ['existing', 'non-existing'],
  n: [60e1],
});

function main({ n, encoding, path }) {
  const enc = encoding === 'undefined' ? undefined : encoding;
  const file = path === 'existing' ? __filename : '/tmp/not-found';
  bench.start();
  for (let i = 0; i < n; ++i) {
    try {
      fs.readFileSync(file, enc);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
