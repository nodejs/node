'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [100],
});

const vm = require('vm');

const ctxFn = new vm.Script(`
  var b = Math.random();
  var c = a + b;
`);

function main({ n }) {
  bench.start();
  let context;
  for (let i = 0; i < n; i++) {
    context = vm.createContext({ a: 'a' });
  }
  bench.end(n);
  ctxFn.runInContext(context);
}
