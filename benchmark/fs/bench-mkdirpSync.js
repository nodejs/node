'use strict';

// Benchmarks MKDirpSync (fs.mkdirSync with recursive: true), which iterates
// over a continuation_data path queue. Varying depth exercises the inner loop
// more, making the continuation_data pointer cache more impactful.

const common = require('../common');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  n: [1e3],
  depth: [4, 8, 16],
});

let dirc = 0;

function main({ n, depth }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    const parts = Array.from({ length: depth }, () => String(++dirc));
    fs.mkdirSync(path.join(tmpdir.path, ...parts), { recursive: true });
  }
  bench.end(n);
}
