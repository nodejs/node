'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e3],
});

function main({ n, type }) {
  let files;

  switch (type) {
    case 'existing':
      files = [];

      // Populate tmpdir with mock files
      for (let i = 0; i < n; i++) {
        const path = tmpdir.resolve(`unlinksync-bench-file-${i}`);
        fs.writeFileSync(path, 'bench');
        files.push(path);
      }
      break;
    case 'non-existing':
      files = new Array(n).fill(tmpdir.resolve(`.non-existing-file-${Date.now()}`));
      break;
    default:
      new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.unlinkSync(files[i]);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
