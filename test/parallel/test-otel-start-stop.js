'use strict';
// Flags: --experimental-otel --expose-internals

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const otel = require('node:otel');
const { getEndpoint } = require('internal/otel/core');

describe('node:otel start/stop API', () => {
  it('start requires an options object', () => {
    assert.throws(() => otel.start('bad'), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  it('start uses default endpoint when omitted', () => {
    otel.start();
    assert.strictEqual(otel.active, true);
    assert.strictEqual(getEndpoint(), 'http://localhost:4318');
    otel.stop();
  });

  it('start uses default endpoint with empty options', () => {
    otel.start({});
    assert.strictEqual(otel.active, true);
    assert.strictEqual(getEndpoint(), 'http://localhost:4318');
    otel.stop();
  });

  it('start rejects non-string endpoint', () => {
    assert.throws(() => otel.start({ endpoint: 42 }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  it('start rejects empty endpoint', () => {
    assert.throws(() => otel.start({ endpoint: '' }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  });

  it('active reflects the tracing state', () => {
    assert.strictEqual(otel.active, false);
    otel.start({ endpoint: 'http://127.0.0.1:1' });
    assert.strictEqual(otel.active, true);
    otel.stop();
    assert.strictEqual(otel.active, false);
  });

  it('stop is a no-op when not active', () => {
    assert.strictEqual(otel.active, false);
    otel.stop(); // Should not throw.
    assert.strictEqual(otel.active, false);
  });

  it('start rejects an invalid URL endpoint', () => {
    assert.throws(() => otel.start({ endpoint: 'not-a-valid-url' }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  });

  it('start rejects an invalid filter type', () => {
    assert.throws(
      () => otel.start({ endpoint: 'http://127.0.0.1:1', filter: 42 }),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  });

  it('start accepts maxBufferSize and flushInterval', () => {
    otel.start({
      endpoint: 'http://127.0.0.1:1',
      maxBufferSize: 50,
      flushInterval: 5000,
    });
    assert.strictEqual(otel.active, true);
    otel.stop();
  });

  it('start rejects non-integer maxBufferSize', () => {
    assert.throws(
      () => otel.start({ endpoint: 'http://127.0.0.1:1', maxBufferSize: 1.5 }),
      { code: 'ERR_OUT_OF_RANGE' },
    );
  });

  it('start rejects non-positive maxBufferSize', () => {
    assert.throws(
      () => otel.start({ endpoint: 'http://127.0.0.1:1', maxBufferSize: 0 }),
      { code: 'ERR_OUT_OF_RANGE' },
    );
  });

  it('start rejects non-integer flushInterval', () => {
    assert.throws(
      () => otel.start({
        endpoint: 'http://127.0.0.1:1',
        flushInterval: 'fast',
      }),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  });

  it('start rejects non-positive flushInterval', () => {
    assert.throws(
      () => otel.start({ endpoint: 'http://127.0.0.1:1', flushInterval: -1 }),
      { code: 'ERR_OUT_OF_RANGE' },
    );
  });

  it('start can be called after stop', () => {
    otel.start({ endpoint: 'http://127.0.0.1:1' });
    assert.strictEqual(otel.active, true);
    otel.stop();
    otel.start({ endpoint: 'http://127.0.0.1:2' });
    assert.strictEqual(otel.active, true);
    otel.stop();
    assert.strictEqual(otel.active, false);
  });
});
