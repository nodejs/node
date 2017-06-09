'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['apply', 'spread', 'call-spread'],
  count: [5, 10, 20],
  context: ['context', 'null'],
  rest: [0, 1],
  millions: [5]
});

function makeTest(count, rest) {
  if (rest) {
    return function test(...args) {
      assert.strictEqual(count, args.length);
    };
  } else {
    return function test() {
      assert.strictEqual(count, arguments.length);
    };
  }
}

function main(conf) {
  const n = +conf.millions * 1e6;
  const ctx = conf.context === 'context' ? {} : null;
  var fn = makeTest(conf.count, conf.rest);
  const args = new Array(conf.count);
  var i;
  for (i = 0; i < conf.count; i++)
    args[i] = i;

  switch (conf.method) {
    case 'apply':
      bench.start();
      for (i = 0; i < n; i++)
        fn.apply(ctx, args);
      bench.end(n / 1e6);
      break;
    case 'spread':
      if (ctx !== null)
        fn = fn.bind(ctx);
      bench.start();
      for (i = 0; i < n; i++)
        fn(...args);
      bench.end(n / 1e6);
      break;
    case 'call-spread':
      bench.start();
      for (i = 0; i < n; i++)
        fn.call(ctx, ...args);
      bench.end(n / 1e6);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
