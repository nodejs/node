'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel HTTP client error span', () => {
  it('creates error span for HTTP client connection refused', async () => {
    let resolvePayload;
    const payloadReceived = new Promise((r) => { resolvePayload = r; });

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

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    // Also need a server to trigger a flush (the error request won't
    // reach a server, so make a successful request too).
    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    // Make a request to a port that is not listening — should fail.
    await new Promise((resolve) => {
      http.get('http://127.0.0.1:1/fail', (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', () => {
        resolve();
      });
    });

    // Make a successful request so we have something to flush.
    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/ok`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const payload = await payloadReceived;

    collector.close();
    server.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;

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
