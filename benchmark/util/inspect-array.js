'use strict';

const common = require('../common');
const util = require('util');

const bench = common.createBenchmark(main, {
  n: [1e2],
  len: [1e5],
  type: [
    'denseArray',
    'sparseArray',
    'mixedArray'
  ]
});

function main(conf) {
  const { n, len, type } = conf;
  var arr = Array(len);
  var i;

  switch (type) {
    case 'denseArray':
      arr = arr.fill(0);
      break;
    case 'sparseArray':
      break;
    case 'mixedArray':
      for (i = 0; i < n; i += 2)
        arr[i] = i;
      break;
    default:
      throw new Error(`Unsupported type ${type}`);
  }
  bench.start();
  for (i = 0; i < n; i++) {
    util.inspect(arr);
  }
  bench.end(n);
}
