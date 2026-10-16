'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const { after, before, describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  startHTTPServer,
  getSpans,
  httpGet,
  spanAttrs,
} = require('../common/otel');

describe('otel HTTP client spans', () => {
  let collector;

  before(async () => {
    collector = await startOTelCollector();
    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });
  });

  after(() => collector.close());

  it('creates client spans and injects traceparent', async () => {
    let receivedTraceparent = null;

    // Target server records incoming traceparent header.
    const target = await startHTTPServer((req, res) => {
      receivedTraceparent = req.headers.traceparent || null;
      res.writeHead(200);
      res.end('ok');
    });

    await httpGet(target.port, '/api/data');
    flush();
    const spans = getSpans(await collector.next());

    await target.close();

    const clientSpan = spans.find((s) => s.kind === 3); // SPAN_KIND_CLIENT
    assert.ok(clientSpan, `Expected a client span in: ${JSON.stringify(spans)}`);
    assert.strictEqual(clientSpan.name, 'GET');

    const attrs = spanAttrs(clientSpan);
    assert.strictEqual(attrs['http.request.method'], 'GET');
    assert.ok(attrs['url.full'], 'Expected url.full attribute');
    assert.strictEqual(attrs['http.response.status_code'], '200');

    assert.ok(receivedTraceparent, 'Expected non-empty traceparent');
    assert.match(receivedTraceparent, /^00-[0-9a-f]{32}-[0-9a-f]{16}-0[01]$/);

    // The traceparent should reference the client span's IDs.
    assert.ok(receivedTraceparent.includes(clientSpan.traceId),
              'traceparent should contain the client span traceId');
    assert.ok(receivedTraceparent.includes(clientSpan.spanId),
              'traceparent should contain the client span spanId');
  });

  // Per OTel semantic conventions, client spans set STATUS_ERROR for >= 400,
  // while server spans only set it for >= 500 (4xx is a client mistake).
  it('sets error on client span but not server span for 404', async () => {
    const server = await startHTTPServer((req, res) => {
      res.writeHead(404);
      res.end('not found');
    });

    await httpGet(server.port, '/missing');
    flush();
    const spans = getSpans(await collector.next());

    await server.close();

    // Find server span (kind SERVER = 1, OTLP wire = 2) and client span
    // (kind CLIENT = 2, OTLP wire = 3).
    const serverSpan = spans.find((s) => {
      return s.kind === 2 && s.attributes?.some(
        (a) => a.key === 'url.path' && a.value.stringValue === '/missing');
    });
    const clientSpan = spans.find((s) => s.kind === 3);

    assert.ok(serverSpan, 'Expected a server span');
    assert.ok(clientSpan, 'Expected a client span');

    // Server span: 404 should NOT have error status.
    assert.strictEqual(serverSpan.status, undefined);

    // Client span: 404 should have error status.
    assert.ok(clientSpan.status, 'Client span should have error status');
    assert.strictEqual(clientSpan.status.code, 2); // STATUS_ERROR
  });

  it('creates error span for HTTP client connection refused', async () => {
    // Make a request to a port that is not listening — should fail.
    await httpGet(1, '/fail').catch(() => {});

    flush();
    const spans = getSpans(await collector.next());

    const errorSpan = spans.find((s) => {
      if (s.kind !== 3) return false; // CLIENT
      const urlAttr = s.attributes?.find((a) => a.key === 'url.full');
      return urlAttr?.value?.stringValue?.includes('/fail');
    });

    assert.ok(errorSpan, 'Expected an error client span for connection refused');

    assert.ok(errorSpan.status, 'Error span should have status');
    assert.strictEqual(errorSpan.status.code, 2); // STATUS_ERROR

    assert.ok(errorSpan.events, 'Error span should have events');
    const exceptionEvent = errorSpan.events.find(
      (e) => e.name === 'exception',
    );
    assert.ok(exceptionEvent, 'Should have exception event');
  });
});
