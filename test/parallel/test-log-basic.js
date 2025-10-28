'use strict';

require('../common');

const assert = require('assert');
const { createLogger, Logger, Handler, JSONHandler, LEVELS } = require('logger');
const fs = require('fs');
const os = require('os');
const path = require('path');

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
}

// Test child logger context inheritance
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

// Test JSONHandler output format
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-1.json`);
  const handler = new JSONHandler({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  const logger = new Logger({ handler });

  logger.info({ msg: 'test message', userId: 123 });

  // Flush synchronously and read the output
  handler.flushSync();
  handler.end();
  const output = fs.readFileSync(tmpfile, 'utf8');

  // Parse the JSON output
  const parsed = JSON.parse(output.trim());
  assert.strictEqual(parsed.level, 'info');
  assert.strictEqual(parsed.msg, 'test message');
  assert.strictEqual(parsed.userId, 123);
  assert.strictEqual(typeof parsed.time, 'number');

  // Cleanup
  fs.unlinkSync(tmpfile);
}

// Test JSONHandler with additional fields
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-2.json`);
  const handler = new JSONHandler({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
    fields: { hostname: 'test-host', pid: 12345 },
  });
  const logger = new Logger({ handler });

  logger.info({ msg: 'with fields' });

  handler.flushSync();
  handler.end();
  const output = fs.readFileSync(tmpfile, 'utf8');
  const parsed = JSON.parse(output.trim());
  assert.strictEqual(parsed.hostname, 'test-host');
  assert.strictEqual(parsed.pid, 12345);
  assert.strictEqual(parsed.msg, 'with fields');

  // Cleanup
  fs.unlinkSync(tmpfile);
}

// Test child logger bindings in output
{
  const tmpfile = path.join(os.tmpdir(), `test-log-${process.pid}-3.json`);
  const handler = new JSONHandler({
    stream: fs.openSync(tmpfile, 'w'),
    level: 'info',
  });
  const logger = new Logger({ handler });
  const childLogger = logger.child({ requestId: 'xyz-789' });

  childLogger.info({ msg: 'child log', action: 'create' });

  handler.flushSync();
  handler.end();
  const output = fs.readFileSync(tmpfile, 'utf8');
  const parsed = JSON.parse(output.trim());
  assert.strictEqual(parsed.requestId, 'xyz-789');
  assert.strictEqual(parsed.action, 'create');
  assert.strictEqual(parsed.msg, 'child log');

  // Cleanup
  fs.unlinkSync(tmpfile);
}

// Test invalid log level
{
  assert.throws(() => {
    createLogger({ level: 'invalid' });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

// Test Handler is abstract
{
  const handler = new Handler({ level: 'info' });
  assert.throws(() => {
    handler.handle({});
  }, {
    message: /must be implemented/,
  });
}

// Test disabled level skips processing
{
  let handleCalled = false;

  class TestHandler extends Handler {
    handle() {
      handleCalled = true;
    }
  }

  const logger = new Logger({
    handler: new TestHandler({ level: 'warn' }),
  });

  // This should be skipped (info < warn)
  logger.info({ msg: 'skipped' });
  assert.strictEqual(handleCalled, false);

  // This should be processed (error > warn)
  logger.error({ msg: 'processed' });
  assert.strictEqual(handleCalled, true);
}
