'use strict';

const common = require('../common.js');
const util = require('util');

const bench = common.createBenchmark(main, {
  method: ['spread', 'assign', '_extend'],
  count: [5, 10, 20],
  n: [1e6]
});

function main({ n, context, count, rest, method }) {

  const src = {};
  for (let n = 0; n < count; n++)
    src[`p${n}`] = n;

  let obj;  // eslint-disable-line no-unused-vars

  switch (method) {
    case '_extend':
      bench.start();
      for (let i = 0; i < n; i++)
        obj = util._extend({}, src);
      bench.end(n);
      break;
    case 'assign':
      bench.start();
      for (let i = 0; i < n; i++)
        obj = Object.assign({}, src);
      bench.end(n);
      break;
    case 'spread':
      bench.start();
      for (let i = 0; i < n; i++)
        obj = { ...src };
      bench.end(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
