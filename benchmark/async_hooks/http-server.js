'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  asyncHooks: ['init', 'before', 'after', 'all', 'disabled', 'none'],
  connections: [50, 500],
  duration: 5,
});

function main({ asyncHooks, connections, duration }) {
  if (asyncHooks !== 'none') {
    let hooks = {
      init() {},
      before() {},
      after() {},
      destroy() {},
      promiseResolve() {},
    };
    if (asyncHooks !== 'all' || asyncHooks !== 'disabled') {
      hooks = {
        [asyncHooks]: () => {},
      };
    }
    const hook = require('async_hooks').createHook(hooks);
    if (asyncHooks !== 'disabled') {
      hook.enable();
    }
  }
  const server = require('../fixtures/simple-http-server.js')
    .listen(common.PORT)
    .on('listening', () => {
      const path = '/buffer/4/4/normal/1';

      bench.http({
        connections,
        path,
        duration,
      }, () => {
        server.close();
      });
    });
}
