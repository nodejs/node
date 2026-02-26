'use strict';
// Flags: --experimental-logger

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { Logger, LogConsumer, JSONConsumer, LEVELS, channels } = require('node:logger');
const { Writable } = require('node:stream');

// Test helper to capture log output
class TestStream extends Writable {
  constructor() {
    super();
    this.logs = [];
  }

  _write(chunk, encoding, callback) {
    this.logs.push(JSON.parse(chunk.toString()));
    callback();
  }

  clear() {
    this.logs = [];
  }

  flush(callback) {
    if (callback) callback();
  }

  flushSync() {
    // No-op for test stream
  }

  end(callback) {
    if (callback) callback();
  }
}

describe('LEVELS constant', () => {
  it('should export correct numeric values for all levels', () => {
    assert.strictEqual(typeof LEVELS, 'object');
    assert.strictEqual(LEVELS.trace, 10);
    assert.strictEqual(LEVELS.debug, 20);
    assert.strictEqual(LEVELS.info, 30);
    assert.strictEqual(LEVELS.warn, 40);
    assert.strictEqual(LEVELS.error, 50);
    assert.strictEqual(LEVELS.fatal, 60);
  });
});

describe('Logger constructor', () => {
  it('should create a Logger instance', () => {
    const logger = new Logger();
    assert(logger instanceof Logger);
  });
});

describe('Logger', () => {
  describe('methods', () => {
    it('should have all log methods', () => {
      const logger = new Logger();
      assert.strictEqual(typeof logger.trace, 'function');
      assert.strictEqual(typeof logger.debug, 'function');
      assert.strictEqual(typeof logger.info, 'function');
      assert.strictEqual(typeof logger.warn, 'function');
      assert.strictEqual(typeof logger.error, 'function');
      assert.strictEqual(typeof logger.fatal, 'function');
      assert.strictEqual(typeof logger.child, 'function');
    });
  });

  describe('level filtering', () => {
    it('should filter logs based on configured level', () => {
      const logger = new Logger({ level: 'warn' });
      assert.strictEqual(logger.trace.enabled, false);
      assert.strictEqual(logger.debug.enabled, false);
      assert.strictEqual(logger.info.enabled, false);
      assert.strictEqual(logger.warn.enabled, true);
      assert.strictEqual(logger.error.enabled, true);
      assert.strictEqual(logger.fatal.enabled, true);
    });
  });

  describe('msg field validation', () => {
    it('should throw when object is missing msg field', () => {
      const logger = new Logger();
      assert.throws(() => {
        logger.info({ userId: 123 }); // Missing msg
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    });

    it('should throw when msg is not a string', () => {
      const logger = new Logger();
      assert.throws(() => {
        logger.info({ msg: 123 }); // msg is not a string
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    });

    it('should accept string message without second argument', () => {
      const logger = new Logger();
      // Should not throw
      logger.info('just a message');
    });
  });

  describe('invalid level', () => {
    it('should throw for invalid log level', () => {
      assert.throws(() => {
        new Logger({ level: 'invalid' });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    });
  });

  describe('invalid fields argument', () => {
    it('should throw when fields is not an object', () => {
      const logger = new Logger();
      assert.throws(() => {
        logger.info('message', 'not an object');
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    });
  });
});

describe('child logger', () => {
  describe('context inheritance', () => {
    it('should create a new Logger instance with inherited level', () => {
      const logger = new Logger({ level: 'info' });
      const childLogger = logger.child({ requestId: 'abc-123' });

      assert(childLogger instanceof Logger);
      assert.notStrictEqual(childLogger, logger);
      assert.strictEqual(childLogger.info.enabled, true);
      assert.strictEqual(childLogger.debug.enabled, false);
    });

    it('should support nested child loggers', () => {
      const logger = new Logger({ level: 'info' });
      const childLogger = logger.child({ requestId: 'abc-123' });
      const grandchildLogger = childLogger.child({ operation: 'query' });

      assert(grandchildLogger instanceof Logger);
      assert.notStrictEqual(grandchildLogger, childLogger);
    });
  });

  describe('level override', () => {
    it('should allow overriding log level in child logger', () => {
      const logger = new Logger({ level: 'info' });
      const childLogger = logger.child({ requestId: 'abc' }, { level: 'debug' });

      assert.strictEqual(logger.debug.enabled, false);
      assert.strictEqual(childLogger.debug.enabled, true);
    });
  });

  describe('bindings in output', () => {
    it('should include child bindings in log output', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      const childLogger = logger.child({ requestId: 'xyz-789' });

      childLogger.info({ msg: 'child log', action: 'create' });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.requestId, 'xyz-789');
      assert.strictEqual(log.action, 'create');
      assert.strictEqual(log.msg, 'child log');
    });
  });

  describe('field merge with consumer fields', () => {
    it('should merge consumer fields, bindings, and log fields', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({
        stream,
        level: 'info',
        fields: { service: 'api' },
      });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      const childLogger = logger.child({ requestId: '123' });

      childLogger.info('request processed', { duration: 150 });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.service, 'api');
      assert.strictEqual(log.requestId, '123');
      assert.strictEqual(log.duration, 150);
      assert.strictEqual(log.msg, 'request processed');
    });
  });

  describe('field override priority', () => {
    it('should prioritize log fields over bindings over consumer fields', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({
        stream,
        level: 'info',
        fields: { env: 'dev', version: '1.0' },
      });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      const childLogger = logger.child({ env: 'staging' });

      childLogger.info('test', { env: 'production' });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.env, 'production');
      assert.strictEqual(log.version, '1.0');
    });
  });
});

describe('LogConsumer', () => {
  it('should be abstract and throw on handle()', () => {
    const consumer = new LogConsumer({ level: 'info' });
    assert.throws(() => {
      consumer.handle({});
    }, {
      code: 'ERR_METHOD_NOT_IMPLEMENTED',
      message: /LogConsumer\.handle\(\) method is not implemented/,
    });
  });

  it('should skip processing when level is disabled', () => {
    let handleCalled = false;

    class TestConsumer extends LogConsumer {
      handle() {
        handleCalled = true;
      }
    }

    const consumer = new TestConsumer({ level: 'warn' });
    consumer.attach();
    const logger = new Logger({ level: 'warn' });

    // This should be skipped (info < warn)
    logger.info({ msg: 'skipped' });
    assert.strictEqual(handleCalled, false);

    // This should be processed (error > warn)
    logger.error({ msg: 'processed' });
    assert.strictEqual(handleCalled, true);
  });
});

describe('JSONConsumer', () => {
  describe('output format', () => {
    it('should output valid JSON with level, time, and msg', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      logger.info({ msg: 'test message', userId: 123 });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.level, 'info');
      assert.strictEqual(log.msg, 'test message');
      assert.strictEqual(log.userId, 123);
      assert.strictEqual(typeof log.time, 'number');
    });
  });

  describe('additional fields', () => {
    it('should include consumer fields in output', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({
        stream,
        level: 'info',
        fields: { hostname: 'test-host', pid: 12345 },
      });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      logger.info({ msg: 'with fields' });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.hostname, 'test-host');
      assert.strictEqual(log.pid, 12345);
      assert.strictEqual(log.msg, 'with fields');
    });
  });

  describe('string message', () => {
    it('should handle string message without fields', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      logger.info('simple message');
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.msg, 'simple message');
      assert.strictEqual(log.level, 'info');
    });

    it('should handle string message with fields', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      logger.info('user login', { userId: 123, ip: '127.0.0.1' });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.msg, 'user login');
      assert.strictEqual(log.userId, 123);
      assert.strictEqual(log.ip, '127.0.0.1');
    });
  });
});

describe('Error serialization', () => {
  describe('err field in object', () => {
    it('should serialize Error with message, code, and stack', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      const err = new Error('test error');
      err.code = 'TEST_ERROR';
      logger.error({ msg: 'operation failed', err });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.msg, 'operation failed');
      assert.strictEqual(typeof log.err, 'object');
      assert.strictEqual(log.err.message, 'test error');
      assert.strictEqual(log.err.code, 'TEST_ERROR');
      assert.ok(log.err.stack);
    });
  });

  describe('Error as first argument', () => {
    it('should use error message as msg and serialize to err field', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      const err = new Error('boom');
      logger.error(err);
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.msg, 'boom');
      assert.strictEqual(typeof log.err, 'object');
      assert.ok(log.err.stack);
    });
  });

  describe('both err and error fields', () => {
    it('should serialize both err and error fields', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      const err = new Error('Error 1');
      const error = new Error('Error 2');

      logger.error({ msg: 'Multiple errors', err, error });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.err.message, 'Error 1');
      assert.strictEqual(log.error.message, 'Error 2');
      assert.ok(log.err.stack);
      assert.ok(log.error.stack);
    });
  });

  describe('error field serialization', () => {
    it('should serialize error field with all properties', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();
      const logger = new Logger({ level: 'info' });

      const error = new Error('Test error');
      error.code = 'TEST_CODE';

      logger.error({ msg: 'Operation failed', error });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.error.message, 'Test error');
      assert.strictEqual(log.error.code, 'TEST_CODE');
      assert.ok(log.error.stack);
    });
  });
});

describe('multiple consumers', () => {
  it('should support multiple consumers with different levels', () => {
    const stream1 = new TestStream();
    const stream2 = new TestStream();

    // Console consumer logs everything (debug+)
    const consumer1 = new JSONConsumer({ stream: stream1, level: 'debug' });
    consumer1.attach();

    // Service consumer logs only warnings+ (warn+)
    const consumer2 = new JSONConsumer({ stream: stream2, level: 'warn' });
    consumer2.attach();

    const logger = new Logger({ level: 'debug' });

    logger.debug({ msg: 'debug message' });
    logger.info({ msg: 'info message' });
    logger.warn({ msg: 'warn message' });
    logger.error({ msg: 'error message' });

    consumer1.flushSync();
    consumer2.flushSync();

    // Consumer1 should have 4 log entries
    assert.strictEqual(stream1.logs.length, 4);
    assert.strictEqual(stream1.logs[0].level, 'debug');
    assert.strictEqual(stream1.logs[1].level, 'info');
    assert.strictEqual(stream1.logs[2].level, 'warn');
    assert.strictEqual(stream1.logs[3].level, 'error');

    // Consumer2 should have 2 log entries
    assert.strictEqual(stream2.logs.length, 2);
    assert.strictEqual(stream2.logs[0].level, 'warn');
    assert.strictEqual(stream2.logs[1].level, 'error');
  });
});

describe('channels export', () => {
  it('should export channel objects for all levels', () => {
    assert.strictEqual(typeof channels, 'object');
    assert.strictEqual(typeof channels.trace, 'object');
    assert.strictEqual(typeof channels.debug, 'object');
    assert.strictEqual(typeof channels.info, 'object');
    assert.strictEqual(typeof channels.warn, 'object');
    assert.strictEqual(typeof channels.error, 'object');
    assert.strictEqual(typeof channels.fatal, 'object');
  });
});
