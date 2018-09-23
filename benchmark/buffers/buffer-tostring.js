'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: ['', 'utf8', 'ascii', 'latin1', 'binary', 'hex', 'UCS-2'],
  args: [0, 1, 2, 3],
  len: [0, 1, 64, 1024],
  n: [1e7]
});

function main({ encoding, args, len, n }) {
  const buf = Buffer.alloc(len, 42);

  if (encoding.length === 0)
    encoding = undefined;

  var i;
  switch (args) {
    case 1:
      bench.start();
      for (i = 0; i < n; i += 1)
        buf.toString(encoding);
      bench.end(n);
      break;
    case 2:
      bench.start();
      for (i = 0; i < n; i += 1)
        buf.toString(encoding, 0);
      bench.end(n);
      break;
    case 3:
      bench.start();
      for (i = 0; i < n; i += 1)
        buf.toString(encoding, 0, len);
      bench.end(n);
      break;
    default:
      bench.start();
      for (i = 0; i < n; i += 1)
        buf.toString();
      bench.end(n);
      break;
  }
}
