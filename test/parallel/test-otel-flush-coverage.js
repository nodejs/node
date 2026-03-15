'use strict';
// Flags: --experimental-otel --expose-internals

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');
const {
  Span,
  SPAN_KIND_INTERNAL,
} = require('internal/otel/span');
const {
  addSpan,
  flush,
  resetCaches,
} = require('internal/otel/flush');

describe('flush.js coverage', () => {
  it('flush is a no-op when buffer is empty', () => {
    const dc = require('diagnostics_channel');
    let requestCreated = false;
    const onRequest = () => { requestCreated = true; };
    dc.subscribe('http.client.request.created', onRequest);

    otel.start({ endpoint: 'http://127.0.0.1:1' });
    flush();

    dc.unsubscribe('http.client.request.created', onRequest);
    otel.stop();

    assert.strictEqual(requestCreated, false);
  });

  it('flush handles spanToOtlp errors gracefully', async () => {
    otel.start({ endpoint: 'http://127.0.0.1:1' });

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
    otel.stop();

    assert.ok(warning.message.includes('serialize boom'));
  });

  it('flush handles JSONStringify errors gracefully', async () => {
    otel.start({ endpoint: 'http://127.0.0.1:1' });

    const warningPromise = new Promise((resolve) => {
      process.on('warning', function onWarning(w) {
        if (w.name === 'OTelExportWarning') {
          process.removeListener('warning', onWarning);
          resolve(w);
        }
      });
    });

    // Create a fake span-like object that spanToOtlp can process, but
    // whose output contains a BigInt which JSONStringify cannot handle.
    const fakeSpan = {
      name: 'bigint-span',
      getAttributes() { return {}; },
      getEvents() { return []; },
      traceId: 'a'.repeat(32),
      spanId: 'b'.repeat(16),
      parentSpanId: null,
      kind: 0,
      startTimeUnixNano: 1n,
      endTimeUnixNano: 2n,
      status: { code: 0, message: '' },
      traceFlags: 0x01,
    };

    addSpan(fakeSpan);
    flush();

    const warning = await warningPromise;
    otel.stop();

    assert.ok(warning.message.includes('Failed to serialize'));
  });

  it('resetCaches clears the span buffer', async () => {
    const http = require('node:http');
    let collectorHit = false;
    const collector = http.createServer((req, res) => {
      collectorHit = true;
      req.resume();
      req.on('end', () => { res.writeHead(200); res.end(); });
    });
    await new Promise((r) => collector.listen(0, r));

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    // Add a span so buffer is non-empty.
    const span = new Span('test', SPAN_KIND_INTERNAL);
    span.end();

    resetCaches();

    flush();

    // Wait for any potential HTTP request to arrive.
    await new Promise((r) => setTimeout(r, 100));

    assert.strictEqual(collectorHit, false);

    otel.stop();
    collector.close();
  });
});
