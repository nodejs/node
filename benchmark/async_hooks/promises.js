'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  method: [
    'trackingEnabled',
    'trackingDisabled',
  ]
});

async function run(n) {
  for (let i = 0; i < n; i++) {
    await new Promise((resolve) => resolve())
    .then(() => { throw new Error('foobar'); })
    .catch((e) => e);
  }
}

function main({ n, method }) {
  const hook = require('async_hooks').createHook({ promiseResolve() {} });
  switch (method) {
    case 'trackingEnabled':
      hook.enable();
      bench.start();
      run(n).then(() => {
        bench.end(n);
      });
      break;
    case 'trackingDisabled':
      bench.start();
      run(n).then(() => {
        bench.end(n);
      });
      break;
    default:
      throw new Error(`Unsupported method "${method}"`);
  }
}
