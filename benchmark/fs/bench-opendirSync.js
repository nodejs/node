'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  bufferSize: [ 4, 32, 128 ],
  n: [1e4],
});

function main({ n, type, bufferSize }) {
  let path;

  switch (type) {
    case 'existing':
      path = 'lib';
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
      const dir = fs.opendirSync(path, { bufferSize });
      dir.closeSync();
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
