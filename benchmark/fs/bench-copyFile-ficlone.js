'use strict';

const common = require('../common');
const { copyFile } = require('fs/promises');
const { constants } = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['invalid', 'valid'],
  method: ['ficlone', 'ficlone_force'],
  n: [1e4],
});

async function main({ n, type }) {
  tmpdir.refresh();
  const dest = tmpdir.resolve(`copy-file-bench-${process.pid}`);
  let path;

  switch (type) {
    case 'invalid':
      path = tmpdir.resolve(`.existing-file-${process.pid}`);
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
      await copyFile(path, dest, constants.COPYFILE_FICLONE_FORCE);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
