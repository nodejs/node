// Flags: --experimental-dtls --no-warnings

// Test: DTLS mutual authentication. A server with requestCert verifies the
// client's certificate; a client that presents no certificate is rejected.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();

// Case 1: the client presents a certificate the server can verify.
{
  const gotServerSession = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    gotServerSession.resolve(session);
  }), {
    cert, key, ca: [ca], requestCert: true, port: 0, host: '127.0.0.1',
  });

  const client = connect('127.0.0.1', server.address.port, {
    cert, key, ca: [ca], rejectUnauthorized: false,
  });

  await client.opened;
  const serverSession = await gotServerSession.promise;
  await serverSession.opened;

  // The server received and verified the client's certificate.
  const clientCert = serverSession.peerCertificate;
  assert.ok(clientCert);
  assert.ok(clientCert.includes('BEGIN CERTIFICATE'));

  await client.close();
  await server.close();
}

// Case 2: the client presents no certificate; the server requires one and
// rejects the handshake.
{
  const server = listen(mustCall(), {
    cert, key, ca: [ca], requestCert: true, port: 0, host: '127.0.0.1',
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca], rejectUnauthorized: false,
  });

  // The exact alert text varies, so assert only that the handshake is rejected.
  await assert.rejects(client.opened, {
    message: /handshake failure/
  });

  // The failed client tears down its internally-owned endpoint.
  await client.endpoint.closed;
  await server.close();
}
