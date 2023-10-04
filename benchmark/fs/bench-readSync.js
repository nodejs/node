'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const tmpfile = { name: tmpdir.resolve(`.existing-file-1M-${process.pid}`),
                  len: 1 * 1024 * 1024 };
const sectorSize = 512;

tmpfile.contents = Buffer.allocUnsafe(tmpfile.len);

for (let offset = 0; offset < tmpfile.len; offset += sectorSize) {
  const fillByte = 256 * Math.random();
  const nBytesToFill = Math.min(sectorSize, tmpfile.len - offset);
  tmpfile.contents.fill(fillByte, offset, offset + nBytesToFill);
}

fs.writeFileSync(tmpfile.name, tmpfile.contents);

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  paramType: ['offset-and-length', 'no-offset-and-length'],
  n: [1e4],
});

function main({ n, type, paramType }) {
  let fd;

  switch (type) {
    case 'existing':
      fd = fs.openSync(tmpfile.name, 'r', 0o666);
      break;
    case 'non-existing':
      fd = 1 << 30;
      break;
    default:
      new Error('Invalid type');
  }

  const length = fs.statSync(tmpfile.name).blksize;
  const buffer = Buffer.alloc(length);

  bench.start();
  switch (paramType) {
    case 'offset-and-length':
      for (let i = 0; i < n; i++) {
        try {
          fs.readSync(fd, buffer, 0, length, 0);
        } catch {
          // Continue regardless of error.
        }
      }
      break;
    case 'no-offset-and-length':
      for (let i = 0; i < n; i++) {
        try {
          fs.readSync(fd, buffer);
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
