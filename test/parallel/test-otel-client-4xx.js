'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

// Per OTel semantic conventions, client spans set STATUS_ERROR for >= 400,
// while server spans only set it for >= 500 (4xx is a client mistake).

describe('node:otel HTTP 4xx status handling', () => {
  it('sets error on client span but not server span for 404', async () => {
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

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    const server = http.createServer((req, res) => {
      res.writeHead(404);
      res.end('not found');
    });
    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/missing`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const spans = await spansReceived;

    collector.close();
    server.close();

    // Find server span (kind SERVER = 1, OTLP wire = 2) and client span
    // (kind CLIENT = 2, OTLP wire = 3).
    const serverSpan = spans.find((s) => s.kind === 2);
    const clientSpan = spans.find((s) => s.kind === 3);

    assert.ok(serverSpan, 'Expected a server span');
    assert.ok(clientSpan, 'Expected a client span');

    // Server span: 404 should NOT have error status.
    assert.strictEqual(serverSpan.status, undefined);

    // Client span: 404 should have error status.
    assert.ok(clientSpan.status, 'Client span should have error status');
    assert.strictEqual(clientSpan.status.code, 2); // STATUS_ERROR
  });
});
