'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['invalid', 'valid'],
  n: [1e4],
});

function main({ n, type }) {
  tmpdir.refresh();
  const dest = tmpdir.resolve(`copy-file-bench-${process.pid}`);
  let path;

  switch (type) {
    case 'invalid':
      path = tmpdir.resolve(`.existing-file-${process.pid}`);
      break;
    case 'valid':
      path = __filename;
      break;
    default:
      throw new Error('Invalid type');
  }
  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.copyFileSync(path, dest);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
