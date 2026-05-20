'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const tmpfile = tmpdir.resolve(`.existing-file-${process.pid}`);
fs.writeFileSync(tmpfile, 'this-is-for-a-benchmark', 'utf8');

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e4],
});

function main({ n, type }) {
  let fd;

  switch (type) {
    case 'existing':
      fd = fs.openSync(tmpfile, 'r', 0o666);
      break;
    case 'non-existing':
      fd = 1 << 30;
      break;
    default:
      new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.fdatasyncSync(fd);
    } catch {
      // do nothing
    }
  }

  bench.end(n);

  if (type === 'existing') fs.closeSync(fd);
}
