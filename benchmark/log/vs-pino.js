'use strict';

// Benchmark: Compare node:log vs Pino
// This benchmark compares the performance of the new node:log module
// against Pino, which is the performance target

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e5],
  logger: ['node-log', 'pino'],
  scenario: ['simple', 'child', 'disabled', 'fields'],
});

function main({ n, logger, scenario }) {
  const nullFd = fs.openSync('/dev/null', 'w');
  let testLogger;
  let logData;

  if (logger === 'node-log') {
    const { createLogger, JSONHandler } = require('node:logger');
    const handler = new JSONHandler({ stream: nullFd, level: 'info' });
    const baseLogger = createLogger({ handler });

    switch (scenario) {
      case 'simple':
        testLogger = baseLogger;
        logData = { msg: 'benchmark test message' };
        break;

      case 'child':
        testLogger = baseLogger.child({ requestId: 'req-123', userId: 456 });
        logData = { msg: 'benchmark test message' };
        break;

      case 'disabled': {
        // Debug logs when handler level is 'warn' (disabled)
        const warnHandler = new JSONHandler({ stream: nullFd, level: 'warn' });
        testLogger = createLogger({ handler: warnHandler, level: 'debug' });
        logData = { msg: 'benchmark test message' };
        break;
      }

      case 'fields':
        testLogger = baseLogger;
        logData = {
          msg: 'benchmark test message',
          field1: 'value1',
          field2: 'value2',
          field3: 'value3',
          field4: 'value4',
          field5: 'value5',
        };
        break;
    }

    bench.start();
    for (let i = 0; i < n; i++) {
      testLogger.info(logData);
    }
    bench.end(n);

    // Don't close FD immediately, let async writes complete

  } else if (logger === 'pino') {
    const pino = require('pino');
    const destination = pino.destination({ dest: nullFd, sync: false });

    switch (scenario) {
      case 'simple':
        testLogger = pino({ level: 'info' }, destination);
        logData = { msg: 'benchmark test message' };
        break;

      case 'child': {
        const baseLogger = pino({ level: 'info' }, destination);
        testLogger = baseLogger.child({ requestId: 'req-123', userId: 456 });
        logData = { msg: 'benchmark test message' };
        break;
      }

      case 'disabled':
        testLogger = pino({ level: 'warn' }, destination);
        logData = { msg: 'benchmark test message' };
        break;

      case 'fields':
        testLogger = pino({ level: 'info' }, destination);
        logData = {
          msg: 'benchmark test message',
          field1: 'value1',
          field2: 'value2',
          field3: 'value3',
          field4: 'value4',
          field5: 'value5',
        };
        break;
    }

    bench.start();
    for (let i = 0; i < n; i++) {
      if (scenario === 'disabled') {
        // Use debug level to test disabled logging
        testLogger.debug(logData);
      } else {
        testLogger.info(logData);
      }
    }
    bench.end(n);

    // Don't close FD immediately for Pino either

  }
}
