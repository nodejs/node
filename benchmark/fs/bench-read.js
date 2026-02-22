'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bufferSize = 1024;
const sectorSize = 512;

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e4],
});

function main({ n, type }) {
  let fd;

  const tmpfile = { name: tmpdir.resolve(`.existing-file-${process.pid}`),
                    len: bufferSize * n };


  tmpfile.contents = Buffer.allocUnsafe(tmpfile.len);

  for (let offset = 0; offset < tmpfile.len; offset += sectorSize) {
    const fillByte = 256 * Math.random();
    const nBytesToFill = Math.min(sectorSize, tmpfile.len - offset);
    tmpfile.contents.fill(fillByte, offset, offset + nBytesToFill);
  }

  fs.writeFileSync(tmpfile.name, tmpfile.contents);

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

  const buffer = Buffer.alloc(bufferSize);


  let i = 0;
  function readSome() {
    if (i++ === n) {
      bench.end(n);
      if (type === 'existing') fs.closeSync(fd);
    } else {
      fs.read(fd, buffer, readSome);
    }
  }

  bench.start();
  readSome();
}
