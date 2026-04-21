// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 header validation (RFC 9114 §4.2-4.3).
// H3V-01: Header names are lowercased on send.
// H3V-02 through H3V-14 (receive-side validations) are handled
// automatically by nghttp3. The library rejects Transfer-Encoding,
// Connection headers, misplaced pseudo-headers, missing required
// pseudo-headers, uppercase header names from peer, etc. These
// validations are always enabled and cannot be disabled. They are
// verified by nghttp3's own test suite.
// This test verifies:
// - H3V-01: Mixed-case header names are lowercased when received
// - Headers with various valid pseudo-header combinations work
// - Custom headers are delivered correctly

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, ok } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const decoder = new TextDecoder();

// H3V-01: Header names are lowercased on send.
// Send headers with mixed case — the server should receive them
// lowercased (buildNgHeaderString lowercases before passing to nghttp3).
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      // H3V-01: All header names should be lowercase regardless
      // of how the client sent them.
      for (const name of Object.keys(headers)) {
        strictEqual(name, name.toLowerCase(),
                    `Header name "${name}" should be lowercase`);
      }

      // Verify specific headers arrived lowercased.
      strictEqual(headers[':method'], 'GET');
      strictEqual(headers[':path'], '/test');
      strictEqual(headers['x-custom-header'], 'Value1');
      strictEqual(headers['content-type'], 'text/plain');
      strictEqual(headers['x-mixed-case'], 'MixedValue');

      // Verify values are NOT lowercased — only names are.
      strictEqual(headers['x-custom-header'], 'Value1');

      this.sendHeaders({
        // Response with mixed-case names — should be lowercased.
        ':status': '200',
        'Content-Type': 'text/html',
        'X-Response-Header': 'ResponseValue',
      });
      this.writer.writeSync('ok');
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      // Mixed-case names — should be lowercased by buildNgHeaderString.
      ':method': 'GET',
      ':path': '/test',
      ':scheme': 'https',
      ':authority': 'localhost',
      'X-Custom-Header': 'Value1',
      'Content-Type': 'text/plain',
      'X-Mixed-Case': 'MixedValue',
    },
    onheaders: mustCall(function(headers) {
      // Client should also receive lowercased response header names.
      strictEqual(headers[':status'], '200');
      strictEqual(headers['content-type'], 'text/html');
      strictEqual(headers['x-response-header'], 'ResponseValue');

      // Verify all names are lowercase.
      for (const name of Object.keys(headers)) {
        strictEqual(name, name.toLowerCase(),
                    `Response header name "${name}" should be lowercase`);
      }
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await Promise.all([stream.closed, serverDone.promise]);
  clientSession.close();
}

// Verify multiple pseudo-header combinations work correctly.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      // All four required pseudo-headers present.
      ok(headers[':method']);
      ok(headers[':path']);
      ok(headers[':scheme']);
      ok(headers[':authority']);

      this.sendHeaders({ ':status': '204' });
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'POST',
      ':path': '/api/data',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '204');
    }),
  });

  await Promise.all([bytes(stream), stream.closed, serverDone.promise]);
  clientSession.close();
}
