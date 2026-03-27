'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel HTTP client spans', () => {
  it('creates client spans and injects traceparent', async () => {
    let receivedTraceparent = null;

    // Target server records incoming traceparent header.
    const target = http.createServer((req, res) => {
      receivedTraceparent = req.headers.traceparent || null;
      res.writeHead(200);
      res.end('ok');
    });

    let resolveSpans;
    const spansReceived = new Promise((r) => { resolveSpans = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        const data = JSON.parse(body);
        const spans = data.resourceSpans[0].scopeSpans[0].spans;
        res.writeHead(200);
        res.end();
        resolveSpans(spans);
      });
    });

    await new Promise((r) => collector.listen(0, r));
    await new Promise((r) => target.listen(0, r));

    const collectorPort = collector.address().port;
    const targetPort = target.address().port;

    otel.start({ endpoint: `http://127.0.0.1:${collectorPort}` });

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${targetPort}/api/data`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const spans = await spansReceived;

    collector.close();
    target.close();

    assert.ok(spans.length >= 1, `Expected at least 1 span, got: ${spans.length}`);

    const clientSpan = spans.find((s) => s.kind === 3); // SPAN_KIND_CLIENT
    assert.ok(clientSpan, `Expected a client span in: ${JSON.stringify(spans)}`);

    const attrs = {};
    for (const a of clientSpan.attributes) {
      attrs[a.key] = a.value.stringValue || a.value.intValue || a.value.doubleValue;
    }
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
});
