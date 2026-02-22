'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('node:otel server request close handling', () => {
  it('ends span with error when client disconnects before response', async () => {
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

    // Create a server that delays its response.
    const server = http.createServer((req, res) => {
      // Don't respond — the client will disconnect first.
      req.on('close', () => {
        // After the client disconnects, make a normal request to trigger flush.
        http.get(`http://127.0.0.1:${server.address().port}/flush`, (r) => {
          r.resume();
          r.on('end', () => otel.stop());
        });
      });
    });

    // Override handler for the /flush path.
    const origHandler = server.listeners('request')[0];
    server.removeAllListeners('request');
    server.on('request', (req, res) => {
      if (req.url === '/flush') {
        res.writeHead(200);
        res.end('ok');
        return;
      }
      origHandler(req, res);
    });

    await new Promise((r) => server.listen(0, r));
    const serverPort = server.address().port;

    // Use a raw TCP socket to connect and send a partial HTTP request,
    // then destroy the connection before the server responds.
    const socket = net.connect(serverPort, '127.0.0.1', () => {
      socket.write('GET /disconnect HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n');
      // Destroy immediately — server never gets to respond.
      setTimeout(() => socket.destroy(), 50);
    });

    const payload = await payloadReceived;

    collector.close();
    server.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;

    const disconnectSpan = spans.find((s) => {
      if (s.kind !== 2) return false; // SERVER
      const pathAttr = s.attributes?.find((a) => a.key === 'url.path');
      return pathAttr?.value?.stringValue === '/disconnect';
    });

    assert.ok(disconnectSpan, 'Expected a server span for /disconnect');
    assert.ok(disconnectSpan.status, 'Span should have error status');
    assert.strictEqual(disconnectSpan.status.code, 2); // STATUS_ERROR
  });
});
