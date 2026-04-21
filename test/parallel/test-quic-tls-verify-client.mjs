// Flags: --experimental-quic --no-warnings

// Test: client certificate verification.
// With verifyClient: true, a client that provides a valid
//         certificate succeeds.
// With verifyClient: true, a client that does NOT provide a
//         certificate fails the handshake.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, ok, rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const serverKey = createPrivateKey(readKey('agent1-key.pem'));
const serverCert = readKey('agent1-cert.pem');
const clientKey = createPrivateKey(readKey('agent2-key.pem'));
const clientCert = readKey('agent2-cert.pem');

// --- TLS-03: Client provides a certificate — handshake succeeds ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    // The server should see the client's certificate.
    ok(serverSession.peerCertificate);
    await serverSession.close();
  }), {
    sni: { '*': { keys: [serverKey], certs: [serverCert] } },
    alpn: ['quic-test'],
    verifyClient: true,
    // Trust the client's self-signed certificate.
    ca: [clientCert],
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    keys: [clientKey],
    certs: [clientCert],
  });

  await Promise.all([clientSession.opened, clientSession.closed]);
  await serverEndpoint.close();
}

// --- TLS-04: Client does NOT provide a certificate — connection fails ---
// In TLS 1.3, client certificate verification happens post-handshake.
// The client's opened promise may resolve (handshake completes), but
// the server then sends a fatal alert (certificate_required) which
// closes both sides with a transport error.
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await rejects(serverSession.closed, {
      code: 'ERR_QUIC_TRANSPORT_ERROR',
    });
  }), {
    sni: { '*': { keys: [serverKey], certs: [serverCert] } },
    alpn: ['quic-test'],
    verifyClient: true,
    onerror: mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
    }),
  });

  // Client connects WITHOUT providing a certificate.
  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    onerror: mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
    }),
  });

  // The client's closed promise rejects with the transport error
  // from the server's certificate_required alert.
  await rejects(clientSession.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });

  await serverEndpoint.close();
}
