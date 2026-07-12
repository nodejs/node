'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e5],
});

function runBench({ n, path }) {
  for (let i = 0; i < n; i++) {
    try {
      const fd = fs.openSync(path, 'r', 0o666);
      fs.closeSync(fd);
    } catch {
      // do nothing
    }
  }
}


function main({ n, type }) {
  let path;

  switch (type) {
    case 'existing':
      path = __filename;
      break;
    case 'non-existing':
      path = tmpdir.resolve(`.non-existing-file-${process.pid}`);
      break;
    default:
      new Error('Invalid type');
  }

  runBench({ n, path });

  bench.start();
  runBench({ n, path });
  bench.end(n);
}
