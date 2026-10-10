'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const net = require('node:net');
const { after, before, describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  startHTTPServer,
  getSpans,
} = require('../common/otel');

// Sends a raw HTTP request with the given traceparent header to the server.
function requestWithTraceparent(port, traceparent) {
  return new Promise((resolve) => {
    const socket = net.connect(port, '127.0.0.1', () => {
      socket.write(
        `GET /test HTTP/1.1\r\n` +
        `Host: 127.0.0.1:${port}\r\n` +
        `traceparent: ${traceparent}\r\n` +
        `Connection: close\r\n` +
        `\r\n`
      );
      socket.on('data', () => {});
      socket.on('end', resolve);
    });
  });
}

describe('otel traceparent validation', () => {
  let collector;

  before(async () => {
    collector = await startOTelCollector();
    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });
  });

  after(() => collector.close());

  it('rejects invalid traceparent headers (non-hex chars)', async () => {
    const server = await startHTTPServer();

    await requestWithTraceparent(server.port,
                                 '00-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ-' +
                                 'ZZZZZZZZZZZZZZZZ-01');

    flush();
    const spans = getSpans(await collector.next());

    await server.close();

    const serverSpan = spans.find(
      (s) => s.kind === 2 && s.attributes?.some(
        (a) => a.key === 'url.path' && a.value.stringValue === '/test'));
    assert.ok(serverSpan, 'Expected a server span');
    assert.match(serverSpan.traceId, /^[0-9a-f]{32}$/);
    assert.notStrictEqual(serverSpan.traceId,
                          'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz');
    // No parentSpanId since the invalid traceparent was rejected.
    assert.ok(!serverSpan.parentSpanId,
              'Should not have parentSpanId from invalid traceparent');
  });

  it('rejects all-zero traceId in traceparent', async () => {
    const server = await startHTTPServer();

    // All-zero traceId is invalid per W3C spec.
    await requestWithTraceparent(server.port,
                                 '00-00000000000000000000000000000000-' +
                                 '0123456789abcdef-01');

    flush();
    const spans = getSpans(await collector.next());

    await server.close();

    const serverSpan = spans.find(
      (s) => s.kind === 2 && s.attributes?.some(
        (a) => a.key === 'url.path' && a.value.stringValue === '/test'));
    assert.ok(serverSpan);
    assert.notStrictEqual(serverSpan.traceId,
                          '00000000000000000000000000000000');
    assert.ok(!serverSpan.parentSpanId);
  });

  it('rejects traceparent versions other than 00', async () => {
    const server = await startHTTPServer();

    // Version 01 is not understood; a fresh trace must be started.
    await requestWithTraceparent(server.port,
                                 '01-abcdef0123456789abcdef0123456789-' +
                                 '0123456789abcdef-01');

    flush();
    const spans = getSpans(await collector.next());

    await server.close();

    const serverSpan = spans.find(
      (s) => s.kind === 2 && s.attributes?.some(
        (a) => a.key === 'url.path' && a.value.stringValue === '/test'));
    assert.ok(serverSpan);
    assert.notStrictEqual(serverSpan.traceId,
                          'abcdef0123456789abcdef0123456789');
    assert.ok(!serverSpan.parentSpanId,
              'Should not have a parent from an unknown version');
  });
});
