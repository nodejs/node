// Flags: --experimental-dtls --no-warnings

// Test: cipher and ECDH-curve selection and validation.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
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

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

// Case 1: a specific cipher is negotiated and reported on both peers. DTLS
// 1.3 suites are named by their TLS_ names.
for (const [versions, CIPHER] of [
  [V12, 'ECDHE-RSA-AES128-GCM-SHA256'],
  [V13, 'TLS_AES_128_GCM_SHA256'],
]) {
  const gotServerSession = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    gotServerSession.resolve(session);
  }), {
    cert, key, port: 0, host: '127.0.0.1', ciphers: CIPHER, ...versions,
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
    ciphers: CIPHER,
    ...versions,
  });

  await client.opened;
  const serverSession = await gotServerSession.promise;
  await serverSession.opened;

  assert.strictEqual(client.cipher.name, CIPHER);
  assert.strictEqual(serverSession.cipher.name, CIPHER);

  await client.close();
  await server.close();
}

// Case 2: an invalid cipher list is rejected.
assert.throws(() => listen(mustNotCall(), {
  cert, key, port: 0, host: '127.0.0.1', ciphers: 'THIS-IS-NOT-A-CIPHER',
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });

// Case 3: a valid ECDH curve completes a handshake.
for (const versions of [V12, V13]) {
  const server = listen(mustCall(), {
    cert, key, port: 0, host: '127.0.0.1', ecdhCurve: 'P-256', ...versions,
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
    ecdhCurve: 'P-256',
    ...versions,
  });

  await client.opened;

  await client.close();
  await server.close();
}

// Case 4: an invalid ECDH curve is rejected.
assert.throws(() => listen(mustNotCall(), {
  cert, key, port: 0, host: '127.0.0.1', ecdhCurve: 'not-a-curve',
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });
