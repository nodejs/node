'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [1e6],
  statSyncType: ['throw', 'noThrow'],
});


function main({ n, statSyncType }) {
  const arg = path.join(__dirname, 'non.existent');

  bench.start();
  for (let i = 0; i < n; i++) {
    if (statSyncType === 'noThrow') {
      fs.statSync(arg, { throwIfNoEntry: false });
    } else {
      try {
        fs.statSync(arg);
      } catch {
        // Continue regardless of error.
      }
    }
  }
  bench.end(n);
}
