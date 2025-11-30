'use strict';

const common = require('../common');
const { createLogger, JSONHandler } = require('node:logger');
const fs = require('node:fs');

const bench = common.createBenchmark(main, {
  n: [1e5],
  level: ['info', 'debug'],
  fields: [0, 5],
  type: ['simple', 'child', 'disabled'],
});

function main({ n, level, fields, type }) {
  // Use /dev/null to avoid I/O overhead in benchmarks
  const nullFd = fs.openSync('/dev/null', 'w');
  const handler = new JSONHandler({ stream: nullFd, level: 'info' });
  const logger = createLogger({ handler, level });

  // Create test data based on fields count
  const logData = { msg: 'benchmark test message' };
  for (let i = 0; i < fields; i++) {
    logData[`field${i}`] = `value${i}`;
  }

  let testLogger;
  switch (type) {
    case 'simple':
      testLogger = logger;
      break;
    case 'child':
      testLogger = logger.child({ requestId: 'bench-123', userId: 456 });
      break;
    case 'disabled': {
      // When level is debug and handler is info, logs will be disabled
      const nullFd2 = fs.openSync('/dev/null', 'w');

      testLogger = createLogger({
        handler: new JSONHandler({ stream: nullFd2, level: 'warn' }),
        level: 'debug',
      });
      break;
    }
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    testLogger.info(logData);
  }
  bench.end(n);

  handler.end();
}
