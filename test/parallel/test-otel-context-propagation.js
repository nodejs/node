'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');
const { describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  startHTTPServer,
  getSpans,
} = require('../common/otel');

// This test verifies W3C trace context propagation:
// An incoming request with a traceparent header causes child spans to
// share the same traceId, and outgoing requests carry the traceparent.

describe('otel context propagation', () => {
  it('propagates trace context across HTTP hops', async () => {
    const incomingTraceId = 'abcdef0123456789abcdef0123456789';
    const incomingSpanId = '0123456789abcdef';
    const incomingTraceparent =
      `00-${incomingTraceId}-${incomingSpanId}-01`;
    const incomingTracestate = 'vendor=value';

    let outgoingTraceparent = null;
    let outgoingTracestate = null;

    const collector = await startOTelCollector();

    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });

    // Backend server that records the outgoing trace context headers.
    const backend = await startHTTPServer((req, res) => {
      outgoingTraceparent = req.headers.traceparent || null;
      outgoingTracestate = req.headers.tracestate || null;
      res.writeHead(200);
      res.end('backend-ok');
    });

    // Frontend server: receives request, makes outgoing call to backend.
    const frontend = await startHTTPServer((req, res) => {
      http.get(`http://127.0.0.1:${backend.port}/backend`, (backendRes) => {
        backendRes.resume();
        backendRes.on('end', () => {
          res.writeHead(200);
          res.end('frontend-ok');
        });
      });
    });

    // Use a raw TCP socket to send the initial request with a custom
    // traceparent header. This bypasses the HTTP client instrumentation
    // which would overwrite the header.
    await new Promise((resolve) => {
      const socket = net.connect(frontend.port, '127.0.0.1', () => {
        socket.write(
          `GET /frontend HTTP/1.1\r\n` +
          `Host: 127.0.0.1:${frontend.port}\r\n` +
          `traceparent: ${incomingTraceparent}\r\n` +
          `tracestate: ${incomingTracestate}\r\n` +
          `Connection: close\r\n` +
          `\r\n`
        );
        socket.on('data', () => {});
        socket.on('end', resolve);
      });
    });

    flush();
    const spans = getSpans(await collector.next());

    await collector.close();
    await frontend.close();
    await backend.close();

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

    // The tracestate must be forwarded verbatim to child requests.
    assert.strictEqual(outgoingTracestate, incomingTracestate);
  });
});
