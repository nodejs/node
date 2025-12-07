'use strict';

const common = require('../common');

const assert = require('node:assert');
const { createLogger, Logger, LogConsumer, JSONConsumer, LEVELS, channels } = require('node:logger');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

// Test LEVELS constant
{
  assert.strictEqual(typeof LEVELS, 'object');
  assert.strictEqual(LEVELS.trace, 10);
  assert.strictEqual(LEVELS.debug, 20);
  assert.strictEqual(LEVELS.info, 30);
  assert.strictEqual(LEVELS.warn, 40);
  assert.strictEqual(LEVELS.error, 50);
  assert.strictEqual(LEVELS.fatal, 60);
}

// Test createLogger returns a Logger instance
{
  const logger = createLogger();
  assert(logger instanceof Logger);
}

// Test Logger has all log methods
{
  const logger = createLogger();
  assert.strictEqual(typeof logger.trace, 'function');
  assert.strictEqual(typeof logger.debug, 'function');
  assert.strictEqual(typeof logger.info, 'function');
  assert.strictEqual(typeof logger.warn, 'function');
  assert.strictEqual(typeof logger.error, 'function');
  assert.strictEqual(typeof logger.fatal, 'function');
  assert.strictEqual(typeof logger.enabled, 'function');
  assert.strictEqual(typeof logger.child, 'function');
}

// Test level filtering
{
  const logger = createLogger({ level: 'warn' });
  assert.strictEqual(logger.enabled('trace'), false);
  assert.strictEqual(logger.enabled('debug'), false);
  assert.strictEqual(logger.enabled('info'), false);
  assert.strictEqual(logger.enabled('warn'), true);
  assert.strictEqual(logger.enabled('error'), true);
  assert.strictEqual(logger.enabled('fatal'), true);
}

// Test msg field is required
{
  const logger = createLogger();

  // Object without msg should throw
  assert.throws(() => {
    logger.info({ userId: 123 }); // Missing msg
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    logger.info({ msg: 123 }); // msg is not a string
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  // String without second arg should work (no assertion needed, just shouldn't throw)
  logger.info('just a message');
}// Test child logger context inheritance
{
  const logger = createLogger({ level: 'info' });
  const childLogger = logger.child({ requestId: 'abc-123' });

  assert(childLogger instanceof Logger);
  assert.notStrictEqual(childLogger, logger);

  // Child should have same level as parent
  assert.strictEqual(childLogger.enabled('info'), true);
  assert.strictEqual(childLogger.enabled('debug'), false);

  // Test nested child
  const grandchildLogger = childLogger.child({ operation: 'query' });
  assert(grandchildLogger instanceof Logger);
  assert.notStrictEqual(grandchildLogger, childLogger);
}

// Test child logger level override
{
  const logger = createLogger({ level: 'info' });
  const childLogger = logger.child({ requestId: 'abc' }, { level: 'debug' });

  assert.strictEqual(logger.enabled('debug'), false);
  assert.strictEqual(childLogger.enabled('debug'), true);
}

// Test JSONConsumer output format
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-1.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });

  logger.info({ msg: 'test message', userId: 123 });

  // Flush synchronously and read the output
  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      assert.strictEqual(parsed.level, 'info');
      assert.strictEqual(parsed.msg, 'test message');
      assert.strictEqual(parsed.userId, 123);
      assert.strictEqual(typeof parsed.time, 'number');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test JSONConsumer with additional fields
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-2.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
    fields: { hostname: 'test-host', pid: 12345 },
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });

  logger.info({ msg: 'with fields' });

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      assert.strictEqual(parsed.hostname, 'test-host');
      assert.strictEqual(parsed.pid, 12345);
      assert.strictEqual(parsed.msg, 'with fields');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test child logger bindings in output
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-3.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });
  const childLogger = logger.child({ requestId: 'xyz-789' });

  childLogger.info({ msg: 'child log', action: 'create' });

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      assert.strictEqual(parsed.requestId, 'xyz-789');
      assert.strictEqual(parsed.action, 'create');
      assert.strictEqual(parsed.msg, 'child log');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test invalid log level
{
  assert.throws(() => {
    createLogger({ level: 'invalid' });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

// Test LogConsumer is abstract
{
  const consumer = new LogConsumer({ level: 'info' });
  assert.throws(() => {
    consumer.handle({});
  }, {
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
    message: /LogConsumer\.handle\(\) method is not implemented/,
  });
}

// Test disabled level skips processing with diagnostics_channel
{
  let handleCalled = false;

  class TestConsumer extends LogConsumer {
    handle() {
      handleCalled = true;
    }
  }

  const consumer = new TestConsumer({ level: 'warn' });
  consumer.attach();
  const logger = createLogger({ level: 'warn' });

  // This should be skipped (info < warn)
  logger.info({ msg: 'skipped' });
  assert.strictEqual(handleCalled, false);

  // This should be processed (error > warn)
  logger.error({ msg: 'processed' });
  assert.strictEqual(handleCalled, true);
}

// Test invalid fields argument
{
  const logger = createLogger();
  assert.throws(() => {
    logger.info('message', 'not an object'); // Second arg must be object
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// Test string message signature
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-string.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });

  logger.info('simple message');

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      assert.strictEqual(parsed.msg, 'simple message');
      assert.strictEqual(parsed.level, 'info');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test string message with fields
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-string-fields.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });

  logger.info('user login', { userId: 123, ip: '127.0.0.1' });

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      assert.strictEqual(parsed.msg, 'user login');
      assert.strictEqual(parsed.userId, 123);
      assert.strictEqual(parsed.ip, '127.0.0.1');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test Error object serialization
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-error.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });

  const err = new Error('test error');
  err.code = 'TEST_ERROR';
  logger.error({ msg: 'operation failed', err });

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      // Error should be serialized with stack trace
      assert.strictEqual(parsed.msg, 'operation failed');
      assert.strictEqual(typeof parsed.err, 'object');
      assert.strictEqual(parsed.err.message, 'test error');
      assert.strictEqual(parsed.err.code, 'TEST_ERROR');
      assert(parsed.err.stack);
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test Error as first argument
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-error-first.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });

  const err = new Error('boom');
  logger.error(err); // Error as first arg

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      assert.strictEqual(parsed.msg, 'boom'); // message from error
      assert.strictEqual(typeof parsed.err, 'object');
      assert(parsed.err.stack);
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test child logger with parent fields merge
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-child-merge.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
    fields: { service: 'api' }, // consumer fields
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });
  const childLogger = logger.child({ requestId: '123' }); // child bindings

  childLogger.info('request processed', { duration: 150 }); // log fields

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      // Merge order: consumer fields < bindings < log fields
      assert.strictEqual(parsed.service, 'api');
      assert.strictEqual(parsed.requestId, '123');
      assert.strictEqual(parsed.duration, 150);
      assert.strictEqual(parsed.msg, 'request processed');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test field override priority
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-override.json`);
  const consumer = new JSONConsumer({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
    fields: { env: 'dev', version: '1.0' },
  });
  consumer.attach();
  const logger = createLogger({ level: 'info' });
  const childLogger = logger.child({ env: 'staging' });

  childLogger.info('test', { env: 'production' });

  consumer.flushSync();
  consumer.end(() => {
    common.mustSucceed(() => {
      const output = fs.readFileSync(tmpfile, 'utf8').trim();
      assert.notStrictEqual(output, '');
      const parsed = JSON.parse(output);
      // Log fields should override everything
      assert.strictEqual(parsed.env, 'production');
      assert.strictEqual(parsed.version, '1.0');
      fs.unlinkSync(tmpfile);
    })();
  });
}

// Test multiple consumers (Qard's use case)
{
  const tmpfile1 = path.join(os.tmpdir(), `test-log-${process.pid}-multi1.json`);
  const tmpfile2 = path.join(os.tmpdir(), `test-log-${process.pid}-multi2.json`);

  // Console consumer logs everything (debug+)
  const consumer1 = new JSONConsumer({
    stream: fs.openSync(tmpfile1, 'w'),
    level: 'debug',
  });
  consumer1.attach();

  // Service consumer logs only warnings+ (warn+)
  const consumer2 = new JSONConsumer({
    stream: fs.openSync(tmpfile2, 'w'),
    level: 'warn',
  });
  consumer2.attach();

  const logger = createLogger({ level: 'debug' });

  logger.debug({ msg: 'debug message' });
  logger.info({ msg: 'info message' });
  logger.warn({ msg: 'warn message' });
  logger.error({ msg: 'error message' });

  consumer1.flushSync();
  consumer2.flushSync();

  let doneCount = 0;
  function done() {
    doneCount++;
    if (doneCount === 2) {
      const output1 = fs.readFileSync(tmpfile1, 'utf8');
      const lines1 = output1.trim().split('\n').filter(Boolean);
      const output2 = fs.readFileSync(tmpfile2, 'utf8');
      const lines2 = output2.trim().split('\n').filter(Boolean);

      // Consumer1 should have 4 log lines if each log is written per line.
      common.mustSucceed(() => {
        assert.strictEqual(lines1.length, 4);
        assert.strictEqual(JSON.parse(lines1[0]).level, 'debug');
        assert.strictEqual(JSON.parse(lines1[1]).level, 'info');
        assert.strictEqual(JSON.parse(lines1[2]).level, 'warn');
        assert.strictEqual(JSON.parse(lines1[3]).level, 'error');

        // Consumer2 should have 2 log lines if each log is written per line.
        assert.strictEqual(lines2.length, 2);
        assert.strictEqual(JSON.parse(lines2[0]).level, 'warn');
        assert.strictEqual(JSON.parse(lines2[1]).level, 'error');

        fs.unlinkSync(tmpfile1);
        fs.unlinkSync(tmpfile2);
      })();
    }
  }

  consumer1.end(done);
  consumer2.end(done);
}

// Test channels export
{
  assert.strictEqual(typeof channels, 'object');
  assert.strictEqual(typeof channels.trace, 'object');
  assert.strictEqual(typeof channels.debug, 'object');
  assert.strictEqual(typeof channels.info, 'object');
  assert.strictEqual(typeof channels.warn, 'object');
  assert.strictEqual(typeof channels.error, 'object');
  assert.strictEqual(typeof channels.fatal, 'object');
}

// Test: Support both 'err' and 'error' fields
{
  const logs = [];
  const consumer = new JSONConsumer({
    stream: {
      write(data) { logs.push(JSON.parse(data)); },
      flush() {},
      flushSync() {},
      end() {},
    },
    level: 'info',
  });
  consumer.attach();

  const logger = new Logger({ level: 'info' });

  const err = new Error('Error 1');
  const error = new Error('Error 2');

  logger.error({ msg: 'Multiple errors', err, error });

  common.mustSucceed(() => {
    assert.strictEqual(logs.length, 1);
    assert.strictEqual(logs[0].err.message, 'Error 1');
    assert.strictEqual(logs[0].error.message, 'Error 2');
    assert.ok(logs[0].err.stack);
    assert.ok(logs[0].error.stack);
  })();
}

// Test: 'error' field serialization
{
  const logs = [];
  const consumer = new JSONConsumer({
    stream: {
      write(data) { logs.push(JSON.parse(data)); },
      flush() {},
      flushSync() {},
      end() {},
    },
    level: 'info',
  });
  consumer.attach();

  const logger = new Logger({ level: 'info' });
  const error = new Error('Test error');
  error.code = 'TEST_CODE';

  logger.error({ msg: 'Operation failed', error });

  common.mustSucceed(() => {
    assert.strictEqual(logs.length, 1);
    assert.strictEqual(logs[0].error.message, 'Test error');
    assert.strictEqual(logs[0].error.code, 'TEST_CODE');
    assert.ok(logs[0].error.stack);
  })();
}
