'use strict';
// Flags: --experimental-logger

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { Logger, JSONConsumer, stdSerializers } = require('node:logger');
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

  // Add flush methods for compatibility
  flush(callback) {
    if (callback) callback();
  }

  flushSync() {
    // No-op for test stream
  }
}

describe('Logger serializers', () => {
  describe('default err serializer', () => {
    it('should serialize Error objects with type, message, code, and stack', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      const error = new Error('Test error');
      error.code = 'TEST_CODE';

      logger.error(error);
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.err.type, 'Error');
      assert.strictEqual(log.err.message, 'Test error');
      assert.strictEqual(log.err.code, 'TEST_CODE');
      assert.ok(log.err.stack);
    });
  });

  describe('custom serializer', () => {
    it('should apply custom serializer to filter sensitive data', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({
        level: 'info',
        serializers: {
          user: (user) => ({
            id: user.id,
            name: user.name,
            // Password is filtered out
          }),
        },
      });

      logger.info({
        msg: 'User action',
        user: { id: 1, name: 'Alice', password: 'secret' },
      });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.user.id, 1);
      assert.strictEqual(log.user.name, 'Alice');
      assert.strictEqual(log.user.password, undefined);
    });
  });

  describe('child logger serializers', () => {
    it('should inherit parent serializers and allow overrides', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const parent = new Logger({
        level: 'info',
        serializers: {
          a: () => 'parent-a',
          b: () => 'parent-b',
        },
      });

      const child = parent.child(
        {},
        {
          serializers: {
            b: () => 'child-b', // Override
            c: () => 'child-c', // New
          },
        }
      );

      child.info({ msg: 'Test message', a: 'test', b: 'test', c: 'test' });
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.a, 'parent-a'); // Inherited
      assert.strictEqual(log.b, 'child-b'); // Overridden
      assert.strictEqual(log.c, 'child-c'); // New
    });
  });

  describe('standard serializers', () => {
    it('should serialize HTTP req/res objects', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({
        level: 'info',
        serializers: {
          req: stdSerializers.req,
          res: stdSerializers.res,
        },
      });

      const mockReq = {
        method: 'GET',
        url: '/test',
        headers: { 'user-agent': 'test' },
        socket: { remoteAddress: '127.0.0.1', remotePort: 12345 },
      };

      const mockRes = {
        statusCode: 200,
        getHeaders: () => ({ 'content-type': 'application/json' }),
      };

      logger.info({ msg: 'HTTP request', req: mockReq, res: mockRes });
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.req.method, 'GET');
      assert.strictEqual(log.req.url, '/test');
      assert.strictEqual(log.res.statusCode, 200);
    });
  });

  describe('error cause serialization', () => {
    it('should serialize nested error causes', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });

      const cause = new Error('Root cause');
      const error = new Error('Main error');
      error.cause = cause;

      logger.error(error);
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.err.message, 'Main error');
      assert.strictEqual(log.err.cause.message, 'Root cause');
      assert.ok(log.err.cause.stack);
    });
  });

  describe('serializer with fields parameter', () => {
    it('should apply serializer when using string message with fields', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({
        level: 'info',
        serializers: {
          data: (data) => ({ sanitized: true, value: data.value }),
        },
      });

      logger.info('Message', { data: { value: 42, secret: 'hidden' } });
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.data.sanitized, true);
      assert.strictEqual(log.data.value, 42);
      assert.strictEqual(log.data.secret, undefined);
    });
  });
});
