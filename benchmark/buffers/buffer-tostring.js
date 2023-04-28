'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: ['', 'utf8', 'ascii', 'latin1', 'hex', 'UCS-2'],
  args: [0, 1, 3],
  len: [1, 64, 1024],
  n: [1e6],
}, {
  combinationFilter: (p) => {
    return (p.args === 0 && p.encoding === '') ||
           (p.args !== 0 && p.encoding !== '');
  },
});

function main({ encoding, args, len, n }) {
  const buf = Buffer.alloc(len, 42);

  if (encoding.length === 0)
    encoding = undefined;

  switch (args) {
    case 1:
      bench.start();
      for (let i = 0; i < n; i += 1)
        buf.toString(encoding);
      bench.end(n);
      break;
    case 2:
      bench.start();
      for (let i = 0; i < n; i += 1)
        buf.toString(encoding, 0);
      bench.end(n);
      break;
    case 3:
      bench.start();
      for (let i = 0; i < n; i += 1)
        buf.toString(encoding, 0, len);
      bench.end(n);
      break;
    default:
      bench.start();
      for (let i = 0; i < n; i += 1)
        buf.toString();
      bench.end(n);
      break;
  }
}
