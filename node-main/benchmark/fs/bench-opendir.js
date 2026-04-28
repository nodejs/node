'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [100],
  dir: [ 'lib', 'test/parallel'],
  mode: [ 'async', 'sync', 'callback' ],
  bufferSize: [ 4, 32, 1024 ],
});

async function main({ n, dir, mode, bufferSize }) {
  const fullPath = path.resolve(__dirname, '../../', dir);

  bench.start();

  let counter = 0;
  for (let i = 0; i < n; i++) {
    if (mode === 'async') {
      const dir = await fs.promises.opendir(fullPath, { bufferSize });
      // eslint-disable-next-line no-unused-vars
      for await (const entry of dir)
        counter++;
    } else if (mode === 'callback') {
      const dir = await fs.promises.opendir(fullPath, { bufferSize });
      await new Promise((resolve, reject) => {
        function read() {
          dir.read((err, entry) => {
            if (err) {
              reject(err);
            } else if (entry === null) {
              resolve(dir.close());
            } else {
              counter++;
              read();
            }
          });
        }

        read();
      });
    } else {
      const dir = fs.opendirSync(fullPath, { bufferSize });
      while (dir.readSync() !== null)
        counter++;
      dir.closeSync();
    }
  }

  bench.end(counter);
}
