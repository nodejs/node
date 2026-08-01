// Flags: --experimental-dtls --no-warnings

// Test: cipher and ECDH-curve selection and validation.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, throws } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const ca = readKey('ca1-cert.pem').toString();

const CIPHER = 'ECDHE-RSA-AES128-GCM-SHA256';

// Case 1: a specific cipher is negotiated and reported on both peers.
{
  const gotServerSession = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    gotServerSession.resolve(session);
  }), { cert, key, port: 0, host: '127.0.0.1', ciphers: CIPHER });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
    ciphers: CIPHER,
  });

  await client.opened;
  const serverSession = await gotServerSession.promise;
  await serverSession.opened;

  strictEqual(client.cipher.name, CIPHER);
  strictEqual(serverSession.cipher.name, CIPHER);

  await client.close();
  await server.close();
}

// Case 2: an invalid cipher list is rejected.
throws(() => listen(mustNotCall(), {
  cert, key, port: 0, host: '127.0.0.1', ciphers: 'THIS-IS-NOT-A-CIPHER',
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });

// Case 3: a valid ECDH curve completes a handshake.
{
  const server = listen(mustCall(), {
    cert, key, port: 0, host: '127.0.0.1', ecdhCurve: 'P-256',
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
    ecdhCurve: 'P-256',
  });

  await client.opened;

  await client.close();
  await server.close();
}

// Case 4: an invalid ECDH curve is rejected.
throws(() => listen(mustNotCall(), {
  cert, key, port: 0, host: '127.0.0.1', ecdhCurve: 'not-a-curve',
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });
