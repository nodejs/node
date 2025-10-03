'use strict';

const common = require('../common.js');
const util = require('util');

const bench = common.createBenchmark(main, {
  method: ['simple_string', 'object_simple', 'object_complex'],
  n: [1e4],
});

function main({ n, method }) {
  const complexObject = {
    name: 'test',
    value: 123,
    nested: {
      array: [1, 2, 3, 4, 5],
      boolean: true,
      null: null,
    },
  };

  bench.start();

  switch (method) {
    case 'simple_string':
      for (let i = 0; i < n; i++) {
        util.log('Hello World');
      }
      break;
    case 'object_simple':
      for (let i = 0; i < n; i++) {
        util.log('Number:', i, 'Boolean:', true);
      }
      break;
    case 'object_complex':
      for (let i = 0; i < n; i++) {
        util.log('Object:', complexObject);
      }
      break;
  }

  bench.end(n);
}
