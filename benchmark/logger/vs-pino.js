'use strict';

const common = require('../common');
const fs = require('node:fs');

const bench = common.createBenchmark(main, {
  n: [1e5],
  logger: ['node-logger', 'pino'],
  scenario: ['simple', 'child', 'disabled', 'fields'],
});

function main({ n, logger, scenario }) {
  const nullFd = fs.openSync('/dev/null', 'w');
  let testLogger;
  let consumer;

  if (logger === 'node-logger') {
    const { createLogger, JSONConsumer } = require('node:logger');

    switch (scenario) {
      case 'simple': {
        consumer = new JSONConsumer({ stream: nullFd, level: 'info' });
        consumer.attach();
        testLogger = createLogger({ level: 'info' });

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.info('benchmark test message');
        }
        bench.end(n);
        break;
      }

      case 'child': {
        consumer = new JSONConsumer({ stream: nullFd, level: 'info' });
        consumer.attach();
        const baseLogger = createLogger({ level: 'info' });
        testLogger = baseLogger.child({ requestId: 'req-123', userId: 456 });

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.info('benchmark test message');
        }
        bench.end(n);
        break;
      }

      case 'disabled': {
        consumer = new JSONConsumer({ stream: nullFd, level: 'warn' });
        consumer.attach();
        testLogger = createLogger({ level: 'warn' });

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.debug('benchmark test message');
        }
        bench.end(n);
        break;
      }

      case 'fields': {
        consumer = new JSONConsumer({ stream: nullFd, level: 'info' });
        consumer.attach();
        testLogger = createLogger({ level: 'info' });

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.info('benchmark test message', {
            field1: 'value1',
            field2: 'value2',
            field3: 'value3',
            field4: 'value4',
            field5: 'value5',
          });
        }
        bench.end(n);
        break;
      }
    }

    if (consumer) {
      consumer.flushSync();
    }
    fs.closeSync(nullFd);

  } else if (logger === 'pino') {
    const pino = require('pino');
    const destination = pino.destination({ dest: nullFd, sync: false });

    switch (scenario) {
      case 'simple': {
        testLogger = pino({ level: 'info' }, destination);

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.info('benchmark test message');
        }
        bench.end(n);
        break;
      }

      case 'child': {
        const baseLogger = pino({ level: 'info' }, destination);
        testLogger = baseLogger.child({ requestId: 'req-123', userId: 456 });

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.info('benchmark test message');
        }
        bench.end(n);
        break;
      }

      case 'disabled': {
        testLogger = pino({ level: 'warn' }, destination);

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.debug('benchmark test message');
        }
        bench.end(n);
        break;
      }

      case 'fields': {
        testLogger = pino({ level: 'info' }, destination);

        bench.start();
        for (let i = 0; i < n; i++) {
          testLogger.info({
            msg: 'benchmark test message',
            field1: 'value1',
            field2: 'value2',
            field3: 'value3',
            field4: 'value4',
            field5: 'value5',
          });
        }
        bench.end(n);
        break;
      }
    }

    destination.flushSync();
  }
}
