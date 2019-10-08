'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [100],
  dir: [ 'lib', 'test/parallel'],
  mode: [ 'async', 'sync' ]
});

function main({ n, dir, mode }) {
  const fullPath = path.resolve(__dirname, '../../', dir);

  (async () => {
    bench.start();

    let counter = 0;
    for (let i = 0; i < n; i++) {
      if (mode === 'async') {
        // eslint-disable-next-line no-unused-vars
        for await (const entry of await fs.promises.opendir(fullPath))
          counter++;
      } else {
        const dir = fs.opendirSync(fullPath);
        while (dir.readSync() !== null)
          counter++;
        dir.closeSync();
      }
    }

    bench.end(counter);
  })();
}
