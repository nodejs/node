// Flags: --experimental-quic --no-warnings

// stream.cancel() abruptly terminates both directions of a stream. On a
// session whose application protocol defines a cancellation code (HTTP/3),
// the RESET_STREAM / STOP_SENDING frames carry H3_REQUEST_CANCELLED
// (RFC 9114 section 4.1.1); other applications use their "no error" code.
// Refs: https://github.com/nodejs/node/issues/65509

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// RFC 9114 H3_REQUEST_CANCELLED.
const H3_REQUEST_CANCELLED = 0x10cn;

// --- An h3 server abandoning a request signals H3_REQUEST_CANCELLED ---
{
  const clientSawReset = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onerror = () => {};
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function() {
      // The server abandons the request without responding.
      this.cancel();
      assert.strictEqual(this.destroyed, true);
      // Cancelling again is a no-op.
      this.cancel();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/test',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
  });
  stream.onerror = () => {};
  stream.onreset = mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
    assert.strictEqual(err.errorCode, H3_REQUEST_CANCELLED);
    clientSawReset.resolve();
  });

  await clientSawReset.promise;
  await clientSession.close();
  await serverEndpoint.close();
}

// --- On a non-h3 application, cancel() uses the "no error" code (0) ---
{
  const clientSawReset = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onerror = () => {};
    serverSession.onstream = mustCall((stream) => {
      stream.cancel();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['test-proto'],
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    alpn: 'test-proto',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    body: 'data',
  });
  stream.onerror = () => {};
  stream.onreset = mustCall((err) => {
    // A reset carrying the "no error" code is surfaced without an error
    // object: the peer terminated the stream cleanly.
    assert.strictEqual(err, undefined);
    clientSawReset.resolve();
  });

  await clientSawReset.promise;
  await clientSession.close();
  await serverEndpoint.close();
}
