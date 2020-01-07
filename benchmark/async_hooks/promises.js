'use strict';
const common = require('../common.js');
const { createHook } = require('async_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6],
  asyncHooks: [
    'enabled',
    'disabled',
  ]
});

async function run(n) {
  for (let i = 0; i < n; i++) {
    await new Promise((resolve) => resolve())
      .then(() => { throw new Error('foobar'); })
      .catch((e) => e);
  }
}

function main({ n, asyncHooks }) {
  const hook = createHook({ promiseResolve() {} });
  if (asyncHooks !== 'disabled') {
    hook.enable();
  }
  bench.start();
  run(n).then(() => {
    bench.end(n);
  });
}
