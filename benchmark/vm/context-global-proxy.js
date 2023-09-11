'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [100000],
});

const vm = require('vm');
const script = new vm.Script(`
  globalThis.foo++;
`);
const context = vm.createContext({ foo: 1 });

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    script.runInContext(context);
  }
  bench.end(n);
}
