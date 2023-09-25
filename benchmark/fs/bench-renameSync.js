'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['invalid', 'valid'],
  n: [1e3],
});

function main({ n, type }) {
  tmpdir.refresh();

  switch (type) {
    case 'invalid': {
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fs.renameSync(tmpdir.resolve(`.non-existing-file-${i}`), tmpdir.resolve(`.new-file-${i}`));
        } catch {
          // do nothing
        }
      }
      bench.end(n);

      break;
    }
    case 'valid': {
      for (let i = 0; i < n; i++) {
        fs.writeFileSync(tmpdir.resolve(`.existing-file-${i}`), 'bench', 'utf8');
      }

      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fs.renameSync(
            tmpdir.resolve(`.existing-file-${i}`),
            tmpdir.resolve(`.new-existing-file-${i}`),
          );
        } catch {
          // do nothing
        }
      }

      bench.end(n);
      break;
    }
    default:
      throw new Error('Invalid type');
  }
}
