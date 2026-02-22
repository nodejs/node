'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel traceparent validation', () => {
  it('rejects invalid traceparent headers (non-hex chars)', async () => {
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
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));
    const serverPort = server.address().port;

    // Send a request with an invalid traceparent (non-hex chars).
    await new Promise((resolve) => {
      const socket = net.connect(serverPort, '127.0.0.1', () => {
        socket.write(
          `GET /test HTTP/1.1\r\n` +
          `Host: 127.0.0.1:${serverPort}\r\n` +
          `traceparent: 00-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ-ZZZZZZZZZZZZZZZZ-01\r\n` +
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
    server.close();

    const serverSpan = spans.find((s) => s.kind === 2);
    assert.ok(serverSpan, 'Expected a server span');
    assert.match(serverSpan.traceId, /^[0-9a-f]{32}$/);
    assert.notStrictEqual(serverSpan.traceId,
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz');
    // No parentSpanId since the invalid traceparent was rejected.
    assert.ok(!serverSpan.parentSpanId,
              'Should not have parentSpanId from invalid traceparent');
  });

  it('rejects all-zero traceId in traceparent', async () => {
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
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));
    const serverPort = server.address().port;

    // All-zero traceId is invalid per W3C spec.
    await new Promise((resolve) => {
      const socket = net.connect(serverPort, '127.0.0.1', () => {
        socket.write(
          `GET /test HTTP/1.1\r\n` +
          `Host: 127.0.0.1:${serverPort}\r\n` +
          `traceparent: 00-00000000000000000000000000000000-0123456789abcdef-01\r\n` +
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
    server.close();

    const serverSpan = spans.find((s) => s.kind === 2);
    assert.ok(serverSpan);
    assert.notStrictEqual(serverSpan.traceId,
                          '00000000000000000000000000000000');
    assert.ok(!serverSpan.parentSpanId);
  });
});
