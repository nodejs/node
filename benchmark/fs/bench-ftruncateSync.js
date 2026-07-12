'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const path = tmpdir.resolve(`new-file-${process.pid}`);
fs.appendFileSync(path, 'Some content.');

const bench = common.createBenchmark(main, {
  type: ['invalid', 'valid'],
  n: [1e4],
});

function main({ n, type }) {
  let fd;

  switch (type) {
    case 'invalid':
      fd = 1 << 30;
      break;
    case 'valid':
      fd = fs.openSync(path, 'r+');
      break;
    default:
      throw new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.ftruncateSync(fd, 4);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
  if (type === 'valid') fs.closeSync(fd);
}
