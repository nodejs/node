'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  startHTTPServer,
  getSpans,
  httpGet,
  spanAttrs,
} = require('../common/otel');

describe('otel HTTP server spans', () => {
  it('creates server spans for incoming HTTP requests', async () => {
    const collector = await startOTelCollector();

    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });

    const app = await startHTTPServer();

    await httpGet(app.port, '/test-path');

    flush();
    const spans = getSpans(await collector.next());

    await collector.close();
    await app.close();

    assert.ok(spans.length >= 1, `Expected at least 1 span, got ${spans.length}`);

    const serverSpan = spans.find((s) => s.kind === 2); // SPAN_KIND_SERVER
    assert.ok(serverSpan, 'Expected a server span');
    assert.strictEqual(serverSpan.name, 'GET');

    assert.ok(serverSpan.traceId);
    assert.strictEqual(serverSpan.traceId.length, 32);
    assert.ok(serverSpan.spanId);
    assert.strictEqual(serverSpan.spanId.length, 16);
    assert.ok(serverSpan.startTimeUnixNano);
    assert.ok(serverSpan.endTimeUnixNano);

    const attrs = spanAttrs(serverSpan);
    assert.strictEqual(attrs['http.request.method'], 'GET');
    assert.strictEqual(attrs['url.path'], '/test-path');
    assert.strictEqual(attrs['http.response.status_code'], '200');
  });
});
