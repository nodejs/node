'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel HTTP server spans', () => {
  it('creates server spans for incoming HTTP requests', async () => {
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
    const collectorPort = collector.address().port;

    otel.start({ endpoint: `http://127.0.0.1:${collectorPort}` });

    const appServer = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => appServer.listen(0, r));
    const appPort = appServer.address().port;

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${appPort}/test-path`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const spans = await spansReceived;

    collector.close();
    appServer.close();

    assert.ok(spans.length >= 1, `Expected at least 1 span, got ${spans.length}`);

    const serverSpan = spans.find((s) => s.kind === 2); // SPAN_KIND_SERVER
    assert.ok(serverSpan, 'Expected a server span');

    assert.ok(serverSpan.traceId);
    assert.strictEqual(serverSpan.traceId.length, 32);
    assert.ok(serverSpan.spanId);
    assert.strictEqual(serverSpan.spanId.length, 16);
    assert.ok(serverSpan.startTimeUnixNano);
    assert.ok(serverSpan.endTimeUnixNano);

    const attrs = {};
    for (const a of serverSpan.attributes) {
      attrs[a.key] = a.value.stringValue || a.value.intValue || a.value.doubleValue;
    }
    assert.strictEqual(attrs['http.request.method'], 'GET');
    assert.strictEqual(attrs['url.path'], '/test-path');
    assert.strictEqual(attrs['http.response.status_code'], '200');
  });
});
