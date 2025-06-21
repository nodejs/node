'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const tmpfile = tmpdir.resolve(`.existing-file-${process.pid}`);
fs.writeFileSync(tmpfile, 'this-is-for-a-benchmark', 'utf8');

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing', 'non-flat-existing'],
  n: [1e6],
});

function main({ n, type }) {
  let path;

  switch (type) {
    case 'existing':
      path = __filename;
      break;
    case 'non-flat-existing':
      path = tmpfile;
      break;
    case 'non-existing':
      path = tmpdir.resolve(`.non-existing-file-${process.pid}`);
      break;
    default:
      new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    fs.existsSync(path);
  }
  bench.end(n);
}
