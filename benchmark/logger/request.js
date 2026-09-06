'use strict';

// A child logger per request, logging a few lines through ConsoleProvider.
// The provider's stream write() is replaced to avoid I/O latency.

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e5],
  lines: [0, 1, 3],
  level: ['info', 'debug'],
}, {
  flags: ['--experimental-logger', '--no-warnings'],
});

function main({ n, lines, level }) {
  const { ConsoleProvider, create } = require('node:logger');

  // With level 'debug' the lines are disabled (the provider is at 'info').
  const provider = new ConsoleProvider({ level: 'info' });
  let bytes = 0;
  provider.stream.write = (data) => {
    bytes += data.length;
    return true;
  };
  const logger = create(provider, {
    name: 'bench',
    bindings: { service: 'api' },
  });

  bench.start();
  for (let i = 0; i < n; i++) {
    const child = logger.child({ requestId: i });
    for (let j = 0; j < lines; j++) {
      child[level]('handled', { step: j });
    }
  }
  bench.end(n);

  if (lines > 0 && level === 'info' && bytes === 0) {
    throw new Error('nothing was serialized');
  }
}
