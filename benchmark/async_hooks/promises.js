'use strict';
const common = require('../common.js');
const { createHook } = require('async_hooks');

let hook;
const tests = {
  disabled() {
    hook = createHook({
      promiseResolve() {}
    });
  },
  enabled() {
    hook = createHook({
      promiseResolve() {}
    }).enable();
  },
  enabledWithDestroy() {
    hook = createHook({
      promiseResolve() {},
      destroy() {}
    }).enable();
  }
};

const bench = common.createBenchmark(main, {
  n: [1e6],
  asyncHooks: [
    'enabled',
    'enabledWithDestroy',
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
  if (hook) hook.disable();
  tests[asyncHooks]();
  bench.start();
  run(n).then(() => {
    bench.end(n);
  });
}
