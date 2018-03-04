'use strict';

const common = require('../common.js');
const util = require('util');

const bench = common.createBenchmark(main, {
  method: ['spread', 'assign', '_extend'],
  count: [5, 10, 20],
  millions: [1],
});

function main({ millions, context, count, rest, method }) {
  const n = millions * 1e6;

  const src = {};
  for (let n = 0; n < count; n++)
    src[`p${n}`] = n;

  let obj;  // eslint-disable-line
  let i;

  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case '_extend':
      bench.start();
      for (i = 0; i < n; i++)
        obj = util._extend({}, src);
      bench.end(n);
      break;
    case 'assign':
      bench.start();
      for (i = 0; i < n; i++)
        obj = Object.assign({}, src);
      bench.end(n);
      break;
    case 'spread':
      bench.start();
      for (i = 0; i < n; i++)
        obj = { ...src };
      bench.end(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
