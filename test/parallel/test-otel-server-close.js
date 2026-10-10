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

describe('otel server request close handling', () => {
  it('ends span with error when client disconnects before response', async () => {
    const collector = await startOTelCollector();

    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });

    // Create a server that delays its response for every path except
    // /flush, which responds normally and is used to trigger an export.
    const server = await startHTTPServer((req, res) => {
      if (req.url === '/flush') {
        res.writeHead(200);
        res.end('ok');
        return;
      }
      // Don't respond — the client will disconnect first.
      req.on('close', () => {
        // After the client disconnects, make a normal request and flush.
        http.get(`http://127.0.0.1:${server.port}/flush`, (r) => {
          r.resume();
          r.on('end', () => flush());
        });
      });
    });

    // Use a raw TCP socket to connect and send a partial HTTP request,
    // then destroy the connection before the server responds.
    const socket = net.connect(server.port, '127.0.0.1', () => {
      socket.write('GET /disconnect HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n');
      // Destroy immediately — server never gets to respond.
      setTimeout(() => socket.destroy(), 50);
    });

    const spans = getSpans(await collector.next());

    await collector.close();
    await server.close();

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
