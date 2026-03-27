'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel restart behavior', () => {
  it('start() while active performs clean restart', async () => {
    let resolvePayload;
    let payloadReceived = new Promise((r) => { resolvePayload = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        res.writeHead(200);
        res.end();
        resolvePayload(JSON.parse(body));
      });
    });

    await new Promise((r) => collector.listen(0, r));
    const collectorPort = collector.address().port;

    otel.start({ endpoint: `http://127.0.0.1:${collectorPort}` });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/first`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    // Call start() again WITHOUT calling stop() — should implicitly stop.
    payloadReceived = new Promise((r) => { resolvePayload = r; });
    otel.start({ endpoint: `http://127.0.0.1:${collectorPort}` });

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/second`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const payload = await payloadReceived;

    collector.close();
    server.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;
    assert.ok(spans.length >= 1, 'Expected spans from second session');

    for (const span of spans) {
      assert.match(span.traceId, /^[0-9a-f]{32}$/);
      assert.match(span.spanId, /^[0-9a-f]{16}$/);
    }
  });
});
