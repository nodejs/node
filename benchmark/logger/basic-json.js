'use strict';

const common = require('../common');
const fs = require('node:fs');

const bench = common.createBenchmark(main, {
  n: [1e5],
  scenario: [
    'string-short',
    'string-long',
    'object-simple',
    'object-nested',
    'object-array',
    'object-mixed',
    'child-logger',
    'disabled-level',
    'error-object',
  ],
});

function main({ n, scenario }) {
  const { Logger, JSONConsumer } = require('node:logger');

  const nullFd = fs.openSync('/dev/null', 'w');
  const consumer = new JSONConsumer({ stream: nullFd, level: 'info' });
  consumer.attach();

  const logger = new Logger({ level: 'info' });

  switch (scenario) {
    case 'string-short': {
      // Simple short string message
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.info('hello');
      }
      bench.end(n);
      break;
    }

    case 'string-long': {
      // Long string message (100 chars)
      const longMsg = 'This is a much longer log message that contains ' +
        'more text to serialize and process during logging operations';
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.info(longMsg);
      }
      bench.end(n);
      break;
    }

    case 'object-simple': {
      // Object with msg and a few string fields
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.info({
          msg: 'user action',
          userId: 'user-123',
          action: 'login',
        });
      }
      bench.end(n);
      break;
    }

    case 'object-nested': {
      // Object with nested structure
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.info({
          msg: 'request completed',
          request: {
            method: 'POST',
            path: '/api/users',
            headers: {
              'content-type': 'application/json',
              'user-agent': 'Mozilla/5.0',
            },
          },
          response: {
            statusCode: 200,
            body: { success: true },
          },
        });
      }
      bench.end(n);
      break;
    }

    case 'object-array': {
      // Object with array fields
      const tags = ['web', 'api', 'auth', 'production'];
      const ids = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.info({
          msg: 'batch operation',
          tags,
          processedIds: ids,
          results: ['success', 'success', 'failed', 'success'],
        });
      }
      bench.end(n);
      break;
    }

    case 'object-mixed': {
      // Mixed types: strings, numbers, booleans, null
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.info({
          msg: 'mixed data',
          stringField: 'value',
          numberField: 42,
          floatField: 3.14159,
          booleanField: true,
          nullField: null,
          timestamp: 1704067200000,
        });
      }
      bench.end(n);
      break;
    }

    case 'child-logger': {
      // Child logger with pre-bound context
      const childLogger = logger.child({
        service: 'api',
        version: '1.0.0',
        env: 'production',
      });
      bench.start();
      for (let i = 0; i < n; i++) {
        childLogger.info({
          msg: 'request',
          requestId: 'req-123',
          duration: 150,
        });
      }
      bench.end(n);
      break;
    }

    case 'disabled-level': {
      // Logging at disabled level (debug when level is info)
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.debug('this will be skipped');
      }
      bench.end(n);
      break;
    }

    case 'error-object': {
      // Logging with Error object
      const error = new Error('Something went wrong');
      error.code = 'ERR_SOMETHING';
      bench.start();
      for (let i = 0; i < n; i++) {
        logger.error({
          msg: 'operation failed',
          err: error,
          operation: 'database-query',
        });
      }
      bench.end(n);
      break;
    }
  }

  consumer.flushSync();
  fs.closeSync(nullFd);
}
