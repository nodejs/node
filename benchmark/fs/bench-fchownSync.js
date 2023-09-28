'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

// Windows does not have `getuid` or `getgid`.
if (process.platform === 'win32') {
  return;
}

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e3],
});

function main({ n, type }) {
  const uid = process.getuid();
  const gid = process.getgid();
  let files;

  switch (type) {
    case 'existing':
      files = [];

      // Populate tmpdir with mock files
      for (let i = 0; i < n; i++) {
        const path = tmpdir.resolve(`fchmodsync-bench-file-${i}`);
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

  const fds = files.map((x) => {
    // Try to open, if not return likely invalid fd (1 << 30)
    try {
      return fs.openSync(x, 'r');
    } catch {
      return 1 << 30;
    }
  });

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      fs.fchownSync(fds[i], uid, gid);
    } catch {
      // do nothing
    }
  }
  bench.end(n);

  for (const x of fds) {
    try {
      fs.closeSync(x);
    } catch {
      // do nothing
    }
  }
}
