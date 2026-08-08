'use strict';
// Flags: --experimental-logger

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { Logger, JSONConsumer, stdSerializers, serialize } = require('node:logger');
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

    it('should handle circular error cause', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });

      const error = new Error('Circular error');
      error.cause = error; // Self-referencing cause

      logger.error(error);
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.err.message, 'Circular error');
      assert.strictEqual(log.err.cause, '[Circular]');
    });

    it('should handle explicit cause: undefined (cause in error is true)', () => {
      // Verify serializeErr directly since JSON.stringify drops undefined values
      const error = new Error('boom', { cause: undefined });
      const serialized = stdSerializers.err(error);

      assert.strictEqual(serialized.message, 'boom');
      // 'cause' in error is true when explicitly set, even if value is undefined
      assert.ok('cause' in serialized);
      assert.strictEqual(serialized.cause, undefined);
    });

    it('should handle explicit cause with a string value', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });

      const error = new Error('boom', { cause: 'some reason' });

      logger.error(error);
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.err.message, 'boom');
      assert.strictEqual(log.err.cause, 'some reason');
    });

    it('should not include cause when not explicitly set', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });

      const error = new Error('no cause');

      logger.error(error);
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.err.message, 'no cause');
      // Cause should NOT be present
      assert.ok(!('cause' in log.err));
    });

    it('should handle deeply nested circular error causes', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });

      const err1 = new Error('Error 1');
      const err2 = new Error('Error 2');
      const err3 = new Error('Error 3');
      err1.cause = err2;
      err2.cause = err3;
      err3.cause = err1; // Circular back to err1

      logger.error(err1);
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.err.message, 'Error 1');
      assert.strictEqual(log.err.cause.message, 'Error 2');
      assert.strictEqual(log.err.cause.cause.message, 'Error 3');
      assert.strictEqual(log.err.cause.cause.cause, '[Circular]');
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

  describe('serialize symbol', () => {
    it('should use serialize symbol for custom object serialization', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      class User {
        constructor(id, name, password) {
          this.id = id;
          this.name = name;
          this.password = password;
        }

        [serialize]() {
          return { id: this.id, name: this.name };
        }
      }

      const logger = new Logger({ level: 'info' });
      const user = new User(123, 'Alice', 'secret123');

      logger.info({ msg: 'User logged in', user });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.user.id, 123);
      assert.strictEqual(log.user.name, 'Alice');
      assert.strictEqual(log.user.password, undefined);
    });

    it('should apply both serialize symbol and field serializer (stacked)', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      class Connection {
        constructor(host, password) {
          this.host = host;
          this.password = password;
        }

        [serialize]() {
          return { host: this.host, type: 'db' };
        }
      }

      const logger = new Logger({
        level: 'info',
        serializers: {
          // This runs AFTER serialize symbol, receives the serialized result
          conn: (c) => ({ ...c, fromSerializer: true }),
        },
      });

      const conn = new Connection('localhost', 'secret');
      logger.info({ msg: 'Connected', conn });
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.conn.host, 'localhost');
      assert.strictEqual(log.conn.type, 'db'); // From [serialize]()
      assert.strictEqual(log.conn.fromSerializer, true); // From field serializer
      assert.strictEqual(log.conn.password, undefined);
    });

    it('should work with child logger bindings', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      class RequestContext {
        constructor(id, internalToken) {
          this.id = id;
          this.internalToken = internalToken;
        }

        [serialize]() {
          return { requestId: this.id };
        }
      }

      const logger = new Logger({ level: 'info' });
      const ctx = new RequestContext('req-456', 'internal-secret');
      const childLogger = logger.child({ ctx });

      childLogger.info({ msg: 'Processing' });
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.ctx.requestId, 'req-456');
      assert.strictEqual(log.ctx.internalToken, undefined);
    });

    it('should handle objects without serialize symbol normally', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      const data = { foo: 'bar', count: 42 };

      logger.info({ msg: 'Plain object', data });
      consumer.flushSync();

      const log = stream.logs[0];
      assert.strictEqual(log.data.foo, 'bar');
      assert.strictEqual(log.data.count, 42);
    });
  });

  describe('JSON escaping', () => {
    it('should properly escape special characters in msg', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      logger.info('My invalid json ", "__proto__": true,');
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.msg, 'My invalid json ", "__proto__": true,');
      assert.ok(!Object.hasOwn(log, '__proto__'));
    });

    it('should properly escape special characters in field keys', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      logger.info({ msg: 'test', ['"break']: 'value' });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log['"break'], 'value');
    });

    it('should properly escape special characters in field values', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      logger.info('test', { data: 'value with "quotes" and \nnewline' });
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log.data, 'value with "quotes" and \nnewline');
    });

    it('should properly escape special characters in bindings keys', () => {
      const stream = new TestStream();
      const consumer = new JSONConsumer({ stream, level: 'info' });
      consumer.attach();

      const logger = new Logger({ level: 'info' });
      const child = logger.child({ ['"break']: 'bla' });
      child.info('test');
      consumer.flushSync();

      assert.strictEqual(stream.logs.length, 1);
      const log = stream.logs[0];
      assert.strictEqual(log['"break'], 'bla');
    });
  });
});

describe('LogConsumer detach', () => {
  it('should stop receiving logs after detach', () => {
    const stream = new TestStream();
    const consumer = new JSONConsumer({ stream, level: 'info' });
    consumer.attach();

    const logger = new Logger({ level: 'info' });

    logger.info('before detach');
    consumer.flushSync();
    assert.strictEqual(stream.logs.length, 1);

    consumer.detach();

    logger.info('after detach');
    consumer.flushSync();
    assert.strictEqual(stream.logs.length, 1); // No new log
  });
});
