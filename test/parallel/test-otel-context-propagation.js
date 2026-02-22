'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');
const { describe, it } = require('node:test');

const otel = require('node:otel');

// This test verifies W3C trace context propagation:
// An incoming request with a traceparent header causes child spans to
// share the same traceId, and outgoing requests carry the traceparent.

describe('node:otel context propagation', () => {
  it('propagates trace context across HTTP hops', async () => {
    const incomingTraceId = 'abcdef0123456789abcdef0123456789';
    const incomingSpanId = '0123456789abcdef';
    const incomingTraceparent =
      `00-${incomingTraceId}-${incomingSpanId}-01`;

    let outgoingTraceparent = null;

    // Backend server that records the outgoing traceparent.
    const backend = http.createServer((req, res) => {
      outgoingTraceparent = req.headers.traceparent || null;
      res.writeHead(200);
      res.end('backend-ok');
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
    await new Promise((r) => backend.listen(0, r));

    const collectorPort = collector.address().port;
    const backendPort = backend.address().port;

    otel.start({ endpoint: `http://127.0.0.1:${collectorPort}` });

    // Frontend server: receives request, makes outgoing call to backend.
    const frontend = http.createServer((req, res) => {
      http.get(`http://127.0.0.1:${backendPort}/backend`, (backendRes) => {
        backendRes.resume();
        backendRes.on('end', () => {
          res.writeHead(200);
          res.end('frontend-ok');
        });
      });
    });

    await new Promise((r) => frontend.listen(0, r));
    const frontendPort = frontend.address().port;

    // Use a raw TCP socket to send the initial request with a custom
    // traceparent header. This bypasses the HTTP client instrumentation
    // which would overwrite the header.
    await new Promise((resolve) => {
      const socket = net.connect(frontendPort, '127.0.0.1', () => {
        socket.write(
          `GET /frontend HTTP/1.1\r\n` +
          `Host: 127.0.0.1:${frontendPort}\r\n` +
          `traceparent: ${incomingTraceparent}\r\n` +
          `Connection: close\r\n` +
          `\r\n`
        );
        socket.on('data', () => {});
        socket.on('end', resolve);
      });
    });

    otel.stop();

    const spans = await spansReceived;

    collector.close();
    frontend.close();
    backend.close();

    assert.ok(spans.length >= 1, `Expected at least 1 span, got: ${spans.length}`);

    const serverSpan = spans.find((s) => {
      if (s.kind !== 2) return false; // SERVER
      const pathAttr = s.attributes?.find((a) => a.key === 'url.path');
      return pathAttr?.value?.stringValue === '/frontend';
    });
    assert.ok(serverSpan, 'Expected a frontend server span');
    assert.strictEqual(serverSpan.traceId, incomingTraceId);

    assert.strictEqual(serverSpan.parentSpanId, incomingSpanId);

    const clientSpan = spans.find((s) => s.kind === 3); // CLIENT
    assert.ok(clientSpan, 'Expected a client span for outgoing backend call');
    assert.strictEqual(clientSpan.traceId, incomingTraceId);

    assert.ok(outgoingTraceparent);
    assert.ok(outgoingTraceparent.includes(incomingTraceId),
              'Outgoing traceparent should carry the original traceId');
  });
});
