'use strict';

// Calls to level methods for a level the provider has disabled.

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e8],
  provider: ['console', 'custom'],
  attributes: ['none', 'object'],
  child: [0, 1],
}, {
  flags: ['--experimental-logger', '--no-warnings'],
});

function main({ n, provider, attributes, child }) {
  const { ConsoleProvider, create } = require('node:logger');

  let logger = create(provider === 'console' ?
    new ConsoleProvider({ level: 'info' }) :
    {
      isEnabled(level) { return level.value >= 30; },
      log() {},
    });
  if (child) logger = logger.child({ requestId: 1 });

  if (attributes === 'object') {
    bench.start();
    for (let i = 0; i < n; i++) logger.debug('hello', { i });
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; i++) logger.debug('hello');
    bench.end(n);
  }
}
