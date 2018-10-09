'use strict';
const common = require('../common.js');
const {
  Duplex,
  Readable,
  Transform,
  Writable,
} = require('stream');

const bench = common.createBenchmark(main, {
  n: [50e6],
  kind: ['duplex', 'readable', 'transform', 'writable']
});

function main({ n, kind }) {
  var i = 0;
  switch (kind) {
    case 'duplex':
      new Duplex({});
      new Duplex();

      bench.start();
      for (; i < n; ++i)
        new Duplex();
      bench.end(n);
      break;
    case 'readable':
      new Readable({});
      new Readable();

      bench.start();
      for (; i < n; ++i)
        new Readable();
      bench.end(n);
      break;
    case 'writable':
      new Writable({});
      new Writable();

      bench.start();
      for (; i < n; ++i)
        new Writable();
      bench.end(n);
      break;
    case 'transform':
      new Transform({});
      new Transform();

      bench.start();
      for (; i < n; ++i)
        new Transform();
      bench.end(n);
      break;
    default:
      throw new Error('Invalid kind');
  }
}
