'use strict';

// Cost of creating loggers and child loggers.

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  type: ['create', 'child'],
}, {
  flags: ['--experimental-logger', '--no-warnings'],
});

function main({ n, type }) {
  const { ConsoleProvider, create } = require('node:logger');

  const provider = new ConsoleProvider();
  const parent = create(provider, { name: 'bench' });
  const loggers = new Array(1024);

  if (type === 'child') {
    bench.start();
    for (let i = 0; i < n; i++) {
      loggers[i & 1023] = parent.child({ requestId: i });
    }
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; i++) {
      loggers[i & 1023] = create(provider, { name: 'bench' });
    }
    bench.end(n);
  }
}
