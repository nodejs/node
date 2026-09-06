'use strict';

// End-to-end cost of logging through ConsoleProvider with the default JSON
// serializer. The provider's stream write() is replaced to avoid I/O latency.

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e5],
  variant: ['json', 'flatten-pid', 'error'],
}, {
  flags: ['--experimental-logger', '--no-warnings'],
});

function main({ n, variant }) {
  const { ConsoleProvider, create } = require('node:logger');

  const provider = new ConsoleProvider({
    flatten: variant === 'flatten-pid',
    pid: variant === 'flatten-pid',
  });
  let bytes = 0;
  provider.stream.write = (data) => {
    bytes += data.length;
    return true;
  };
  const logger = create(provider, {
    name: 'bench',
    bindings: { service: 'api' },
  });

  if (variant === 'error') {
    const err = new Error('boom');
    err.code = 'E_BOOM';
    bench.start();
    for (let i = 0; i < n; i++) logger.error('failed', { err });
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; i++) logger.info('hello', { i, user: 'x' });
    bench.end(n);
  }

  if (bytes === 0) throw new Error('nothing was serialized');
}
