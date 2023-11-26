'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const path = tmpdir.resolve(`new-file-${process.pid}`);
fs.writeFileSync(path, 'Some content.');

const bench = common.createBenchmark(main, {
  type: ['valid', 'invalid'],
  dataType: ['string', 'buffer'],
  n: [1e5],
});

const stringData = 'Benchmark data.';
const bufferData = Buffer.from(stringData);

function main({ n, type, dataType }) {
  const fd = type === 'valid' ? fs.openSync(path, 'r+') : 1 << 30;
  const data = dataType === 'string' ? stringData : bufferData;

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.writeSync(fd, data);
    } catch {
      // Continue regardless of error.
    }
  }
  bench.end(n);
  if (type === 'valid') fs.closeSync(fd);
}
