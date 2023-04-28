'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['apply', 'spread', 'call-spread'],
  count: [5, 10, 20],
  context: ['context', 'null'],
  rest: [0, 1],
  n: [5e6],
});

function makeTest(count, rest) {
  if (rest) {
    return function test(...args) {
      assert.strictEqual(count, args.length);
    };
  }
  return function test() {
    assert.strictEqual(count, arguments.length);
  };
}

function main({ n, context, count, rest, method }) {
  const ctx = context === 'context' ? {} : null;
  let fn = makeTest(count, rest);
  const args = new Array(count);

  for (let i = 0; i < count; i++)
    args[i] = i;

  switch (method) {
    case 'apply':
      bench.start();
      for (let i = 0; i < n; i++)
        fn.apply(ctx, args);
      bench.end(n);
      break;
    case 'spread':
      if (ctx !== null)
        fn = fn.bind(ctx);
      bench.start();
      for (let i = 0; i < n; i++)
        fn(...args);
      bench.end(n);
      break;
    case 'call-spread':
      bench.start();
      for (let i = 0; i < n; i++)
        fn.call(ctx, ...args);
      bench.end(n);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
