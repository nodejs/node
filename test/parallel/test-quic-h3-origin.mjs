// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 ORIGIN frames (RFC 9412).
// Server with SNI entries sends ORIGIN frame
// Wildcard (*) SNI entries excluded from ORIGIN
// Client receives ORIGIN frame via onorigin callback

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

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// Server sends ORIGIN frame based on SNI entries.
// Wildcard entries are excluded.
{
  const originReceived = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: {
      // Wildcard entry should NOT appear in ORIGIN frame.
      '*': { keys: [key], certs: [cert] },
      // These specific hostnames should appear in ORIGIN.
      'example.com': { keys: [key], certs: [cert] },
      'api.example.com': { keys: [key], certs: [cert] },
    },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('ok'));
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'example.com',
    // Client receives ORIGIN frame via onorigin callback.
    onorigin: mustCall(function(origins) {
      ok(Array.isArray(origins));
      // The origins should include the specific SNI hostnames.
      ok(origins.length >= 2);
      // The wildcard (*) should NOT be in the list.
      const originStrings = origins.join(',');
      ok(originStrings.includes('example.com'), 'should include example.com');
      ok(originStrings.includes('api.example.com'),
         'should include api.example.com');
      ok(!originStrings.includes('*'), 'should not include wildcard');
      originReceived.resolve();
    }),
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'example.com',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');

  await Promise.all([originReceived.promise, stream.closed, serverDone.promise]);
  clientSession.close();
}

// port: 8443 produces origin "https://hostname:8443"
// default port (443) omits port from origin string
// authoritative: false excluded from ORIGIN frame
// authoritative: true (default) included in ORIGIN frame
{
  const originReceived = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: {
      '*': { keys: [key], certs: [cert] },
      // Non-default port → origin includes port.
      'custom-port.example.com': { keys: [key], certs: [cert], port: 8443 },
      // Default port (443) → origin omits port.
      'default-port.example.com': { keys: [key], certs: [cert], port: 443 },
      // authoritative: false → excluded from ORIGIN frame.
      'not-authoritative.example.com': {
        keys: [key], certs: [cert], authoritative: false,
      },
      // authoritative: true (explicit) → included.
      'authoritative.example.com': {
        keys: [key], certs: [cert], authoritative: true,
      },
      // Authoritative defaults to true when omitted.
      'default-auth.example.com': { keys: [key], certs: [cert] },
    },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('ok'));
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'custom-port.example.com',
    onorigin: mustCall(function(origins) {
      ok(Array.isArray(origins));

      // Custom port included in origin string.
      ok(origins.includes('https://custom-port.example.com:8443'),
         'should include origin with custom port');

      // Default port 443 omitted from origin string.
      ok(origins.includes('https://default-port.example.com'),
         'should include origin without port for 443');
      // Verify port 443 is NOT appended.
      const defaultPortOrigin = origins.find((o) =>
        o.includes('default-port.example.com'));
      ok(!defaultPortOrigin.includes(':443'),
         'default port 443 should be omitted');

      // Non-authoritative entry excluded.
      const allOrigins = origins.join(',');
      ok(!allOrigins.includes('not-authoritative'),
         'non-authoritative entry should be excluded');

      // Explicitly authoritative entry included.
      ok(allOrigins.includes('authoritative.example.com'),
         'explicitly authoritative entry should be included');

      // Default authoritative (true when omitted) included.
      ok(allOrigins.includes('default-auth.example.com'),
         'default authoritative entry should be included');

      originReceived.resolve();
    }),
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'custom-port.example.com',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');

  await Promise.all([originReceived.promise, stream.closed, serverDone.promise]);
  clientSession.close();
}
