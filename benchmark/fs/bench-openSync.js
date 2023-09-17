'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e5],
});

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

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      const fd = fs.openSync(path, 'r', 0o666);
      fs.closeSync(fd);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
