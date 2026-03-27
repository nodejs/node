'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel undici spans', () => {
  it('creates client spans for fetch requests', async () => {
    const target = http.createServer((req, res) => {
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

    otel.start({
      endpoint: `http://127.0.0.1:${collectorPort}`,
      filter: ['node:undici', 'node:fetch'],
    });

    try {
      const res = await fetch(`http://127.0.0.1:${targetPort}/fetch-test`);
      await res.text();
    } catch {
      // Ignore fetch errors.
    }

    otel.stop();

    const spans = await spansReceived;

    collector.close();
    target.close();

    assert.ok(spans.length > 0, 'Expected at least one span from fetch');
    const clientSpan = spans.find((s) => s.kind === 3); // CLIENT
    assert.ok(clientSpan, 'Expected a CLIENT span from fetch');

    const attrs = {};
    for (const a of clientSpan.attributes) {
      attrs[a.key] = a.value.stringValue || a.value.intValue;
    }
    assert.strictEqual(attrs['http.request.method'], 'GET');
    assert.ok(attrs['url.full'], 'Expected url.full attribute');
  });
});
