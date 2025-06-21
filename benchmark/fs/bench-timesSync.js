'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  func: [ 'utimes', 'futimes', 'lutimes' ],
  n: [1e3],
});

function main({ n, type, func }) {
  const useFds = func === 'futimes';
  const fsFunc = fs[func + 'Sync'];

  switch (type) {
    case 'existing': {
      const files = [];

      // Populate tmpdir with mock files
      for (let i = 0; i < n; i++) {
        const path = tmpdir.resolve(`timessync-bench-file-${i}`);
        fs.writeFileSync(path, 'bench');
        files.push(useFds ? fs.openSync(path, 'r+') : path);
      }

      bench.start();
      for (let i = 0; i < n; i++) {
        fsFunc(files[i], i, i);
      }
      bench.end(n);

      if (useFds) files.forEach((x) => {
        try {
          fs.closeSync(x);
        } catch {
          // do nothing
        }
      });

      break;
    }
    case 'non-existing': {
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fsFunc(useFds ? (1 << 30) : tmpdir.resolve(`.non-existing-file-${Date.now()}`), i, i);
        } catch {
          // do nothing
        }
      }
      bench.end(n);

      break;
    }
    default:
      new Error('Invalid type');
  }
}
