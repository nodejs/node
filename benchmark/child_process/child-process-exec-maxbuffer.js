'use strict';
const common = require('../common.js');
const { execFile } = require('child_process');

// Isolates stdout accumulation + maxBuffer handling in execFile(). The child
// writes `chunks` blocks of 64 KiB; the parent accumulates them through the
// native pipe read path and the JS buffering in lib/child_process.js until the
// process exits and the result buffer is handed to the callback.

const bench = common.createBenchmark(main, {
  // Number of 64 KiB blocks written by the child: 1 MiB, 16 MiB, 64 MiB.
  chunks: [16, 256, 1024],
  n: [10],
});

function main({ n, chunks }) {
  const script =
    'const b = Buffer.alloc(65536, 0x61);' +
    `for (let i = 0; i < ${chunks}; i++) process.stdout.write(b);`;
  const args = ['-e', script];
  const options = {
    maxBuffer: chunks * 65536 + 65536,
    encoding: 'buffer',
  };

  let left = n;
  const run = () => {
    execFile(process.execPath, args, options, (err) => {
      if (err)
        throw err;
      if (--left === 0)
        return bench.end(n);
      run();
    });
  };

  bench.start();
  run();
}
