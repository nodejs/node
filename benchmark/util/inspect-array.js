'use strict';

const common = require('../common');
const util = require('util');

const bench = common.createBenchmark(main, {
  n: [1e3],
  len: [1e5],
  type: [
    'denseArray',
    'sparseArray',
    'mixedArray',
    'denseArray_showHidden',
  ]
});

function main({ n, len, type }) {
  var arr = Array(len);
  var i, opts;

  // For testing, if supplied with an empty type, default to denseArray.
  type = type || 'denseArray';

  switch (type) {
    case 'denseArray_showHidden':
      opts = { showHidden: true };
      arr = arr.fill('denseArray');
      break;
    case 'denseArray':
      arr = arr.fill('denseArray');
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
    util.inspect(arr, opts);
  }
  bench.end(n);
}
