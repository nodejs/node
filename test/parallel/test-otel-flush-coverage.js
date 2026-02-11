'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const { after, afterEach, before, describe, it } = require('node:test');

const otel = require('internal/otel/core');
const {
  Span,
  SPAN_KIND_INTERNAL,
} = require('internal/otel/span');
const {
  addSpan,
  flush,
  resetCaches,
} = require('internal/otel/flush');
const { startOTelCollector, delay } = require('../common/otel');

describe('flush.js coverage', () => {
  let collector;

  before(async () => {
    collector = await startOTelCollector();
    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });
  });

  after(() => collector.close());

  afterEach(() => {
    // Clear the buffer and the export failure/warning counters so that
    // tests do not interfere with each other.
    resetCaches();
  });

  it('flush is a no-op when buffer is empty', async () => {
    flush();

    // Wait for any potential HTTP request to arrive.
    await delay(100);

    assert.strictEqual(collector.requests.length, 0);
  });

  it('flush handles spanToOtlp errors gracefully', async () => {
    const warningPromise = new Promise((resolve) => {
      process.on('warning', function onWarning(w) {
        if (w.name === 'OTelExportWarning') {
          process.removeListener('warning', onWarning);
          resolve(w);
        }
      });
    });

    // Create a poisoned span-like object that will throw during serialization.
    const badSpan = {
      name: 'bad-span',
      getAttributes() { throw new Error('serialize boom'); },
    };

    addSpan(badSpan);
    flush();

    const warning = await warningPromise;

    assert.ok(warning.message.includes('serialize boom'));
  });

  it('flush handles JSONStringify errors gracefully', async () => {
    const warningPromise = new Promise((resolve) => {
      process.on('warning', function onWarning(w) {
        if (w.name === 'OTelExportWarning') {
          process.removeListener('warning', onWarning);
          resolve(w);
        }
      });
    });

    // Create a fake span-like object that spanToOtlp can process, but whose
    // name is a BigInt: it is copied into the OTLP structure untouched and
    // makes JSONStringify throw.
    const fakeSpan = {
      name: 1n,
      getAttributes() { return {}; },
      getEvents() { return []; },
      traceId: 'a'.repeat(32),
      spanId: 'b'.repeat(16),
      parentSpanId: null,
      kind: 0,
      startTime: 1,
      endTime: 2,
      status: { code: 0, message: '' },
      traceFlags: 0x01,
    };

    addSpan(fakeSpan);
    flush();

    const warning = await warningPromise;

    assert.ok(warning.message.includes('Failed to serialize'));
  });

  it('resetCaches clears the span buffer', async () => {
    // Add a span so the buffer is non-empty.
    const span = new Span('test', SPAN_KIND_INTERNAL);
    span.end();

    resetCaches();

    flush();

    // Wait for any potential HTTP request to arrive.
    await delay(100);

    assert.strictEqual(collector.requests.length, 0);
  });
});
