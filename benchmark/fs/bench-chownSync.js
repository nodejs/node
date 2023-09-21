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
  const uid = process.getuid();
  const gid = process.getgid();
  let path;

  switch (type) {
    case 'existing':
      path = tmpfile;
      break;
    case 'non-existing':
      path = tmpdir.resolve(`.non-existing-file-${Date.now()}`);
      break;
    default:
      new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.chownSync(path, uid, gid);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
