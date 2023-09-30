'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['invalid-mode', 'invalid-path', 'valid'],
  n: [1e4],
});

function main({ n, type }) {
  tmpdir.refresh();
  const dest = tmpdir.resolve(`copy-file-bench-${process.pid}`);
  let path, mode;

  switch (type) {
    case 'invalid-path':
      path = tmpdir.resolve(`.existing-file-${process.pid}`);
      break;
    case 'invalid-mode':
      path = __filename;
      mode = -1;
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
      fs.copyFileSync(path, dest, mode);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
