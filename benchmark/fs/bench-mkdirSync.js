'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  recursive: ['true', 'false'],
  n: [1e3],
});

function main({ n, type, recursive }) {
  recursive = recursive === 'true';
  let files;

  switch (type) {
    case 'non-existing':
      files = [];

      // Populate tmpdir with target dirs
      for (let i = 0; i < n; i++) {
        const path = tmpdir.resolve(recursive ? `rmdirsync-bench-dir-${process.pid}-${i}/a/b/c` : `rmdirsync-bench-dir-${process.pid}-${i}`);
        files.push(path);
      }
      break;
    case 'existing':
      files = new Array(n).fill(__dirname);
      break;
    default:
      new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.mkdirSync(files[i], { recursive });
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
