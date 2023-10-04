'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const tmpfile = tmpdir.resolve(`.existing-file-${process.pid}`);
fs.writeFileSync(tmpfile, 'this-is-for-a-benchmark', 'utf8');

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  paramType: ['offset-and-length', 'no-offset-and-length'],
  n: [1e4],
});

function main({ n, type, paramType }) {
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
  switch (paramType) {
    case 'offset-and-length':
      for (let i = 0; i < n; i++) {
        try {
          fs.readSync(fd, Buffer.alloc(1), 0, 1, 0);
        } catch {
          // Continue regardless of error.
        }
      }
      break;
    case 'no-offset-and-length':
      for (let i = 0; i < n; i++) {
        try {
          fs.readSync(fd, Buffer.alloc(1));
        } catch {
          // Continue regardless of error.
        }
      }
      break;
    default:
      new Error('Invalid type');
  }
  bench.end(n);

  if (type === 'existing') fs.closeSync(fd);
}
