'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  asyncHooks: ['enabed', 'disabled', 'none'],
  c: [50, 500]
});

function main({ asyncHooks, c }) {
  if (asyncHooks !== 'none') {
    const hook = require('async_hooks').createHook({
      init() {},
      before() {},
      after() {},
      destroy() {},
      promiseResolve() {}
    });
    if (asyncHooks === 'enabed') {
      hook.enable();
    }
  }
  const server = require('../fixtures/simple-http-server.js')
  .listen(common.PORT)
  .on('listening', () => {
    const path = '/buffer/4/4/normal/1';

    bench.http({
      path: path,
      connections: c
    }, () => {
      server.close();
    });
  });
}
