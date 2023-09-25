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
  switch (type) {
    case 'existing': {
      for (let i = 0; i < n; i++) {
        fs.writeFileSync(tmpdir.resolve(`chmodsync-bench-file-${i}`), 'bench');
      }

      bench.start();
      for (let i = 0; i < n; i++) {
        fs.chmodSync(tmpdir.resolve(`chmodsync-bench-file-${i}`), 0o666);
      }
      bench.end(n);
      break;
    }
    case 'non-existing':
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fs.chmodSync(tmpdir.resolve(`chmod-non-existing-file-${i}`), 0o666);
        } catch {
          // do nothing
        }
      }
      bench.end(n);
      break;
    default:
      new Error('Invalid type');
  }
}
