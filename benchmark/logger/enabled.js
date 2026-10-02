'use strict';

// Cost of composing an enabled log event, with a provider that does no work.

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  provider: ['minimal', 'custom'],
  variant: ['no-attributes', 'attributes', 'function-binding'],
}, {
  flags: ['--experimental-logger', '--no-warnings'],
});

function main({ n, provider, variant }) {
  const { create } = require('node:logger');
  const { AsyncLocalStorage } = require('node:async_hooks');

  let last;
  const log = (event) => { last = event; };
  const als = new AsyncLocalStorage();
  const bindings = variant === 'function-binding' ?
    { service: 'api', request: () => als.getStore() } :
    { service: 'api' };
  const logger = create(provider === 'minimal' ? { log } : {
    isEnabled(level) { return level.value >= 30; },
    log,
  }, { name: 'bench', bindings });

  switch (variant) {
    case 'no-attributes':
      bench.start();
      for (let i = 0; i < n; i++) logger.info('hello');
      bench.end(n);
      break;
    case 'attributes':
      bench.start();
      for (let i = 0; i < n; i++) logger.info('hello', { i, user: 'x' });
      bench.end(n);
      break;
    case 'function-binding':
      als.enterWith(1);
      bench.start();
      for (let i = 0; i < n; i++) logger.info('hello', { i });
      bench.end(n);
      break;
  }

  if (last === undefined) throw new Error('no event was logged');
}
