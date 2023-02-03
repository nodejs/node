'use strict';
const common = require('../common.js');
const {
  Duplex,
  Readable,
  Transform,
  Writable,
} = require('stream');

const bench = common.createBenchmark(main, {
  n: [1e6],
  kind: ['duplex', 'readable', 'transform', 'writable'],
});

function main({ n, kind }) {
  switch (kind) {
    case 'duplex':
      new Duplex({});
      new Duplex();

      bench.start();
      for (let i = 0; i < n; ++i)
        new Duplex().destroy();
      bench.end(n);
      break;
    case 'readable':
      new Readable({});
      new Readable();

      bench.start();
      for (let i = 0; i < n; ++i)
        new Readable().destroy();
      bench.end(n);
      break;
    case 'writable':
      new Writable({});
      new Writable();

      bench.start();
      for (let i = 0; i < n; ++i)
        new Writable().destroy();
      bench.end(n);
      break;
    case 'transform':
      new Transform({});
      new Transform();

      bench.start();
      for (let i = 0; i < n; ++i)
        new Transform().destroy();
      bench.end(n);
      break;
    default:
      throw new Error('Invalid kind');
  }
}
