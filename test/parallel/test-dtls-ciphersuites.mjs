// Flags: --experimental-dtls --no-warnings

// Test: the ciphers option configures DTLS 1.3 cipher suites as well as the
// DTLS 1.2 cipher list.
//
// OpenSSL keeps the two apart -- SSL_CTX_set_cipher_list() has no effect on
// TLS 1.3 suites -- so, as in node:tls, TLS_ names in the string are applied
// as DTLS 1.3 suites and everything else as the DTLS 1.2 list.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, createSecureContext, listen } = await import('node:dtls');

const HOST = '127.0.0.1';
const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };
const BOTH = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.3' };

async function handshake(serverOptions, clientOptions) {
  const serverCipher = Promise.withResolvers();
  const endpoint = listen((session) => {
    session.onhandshake = () => serverCipher.resolve(session.cipher.name);
    session.onerror = () => {};
  }, { cert, key, host: HOST, port: 0, ...serverOptions });
  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, handshakeTimeout: 2000, ...clientOptions,
  });
  try {
    const { protocol } = await client.opened;
    const cipher = client.cipher.name;
    assert.strictEqual(await serverCipher.promise, cipher);
    await client.close();
    return { protocol, cipher };
  } finally {
    client.destroy();
    await endpoint.close();
  }
}

// A DTLS 1.3 suite is selected by name, on either side.
for (const suite of ['TLS_AES_128_GCM_SHA256', 'TLS_CHACHA20_POLY1305_SHA256']) {
  assert.deepStrictEqual(
    await handshake({ ciphers: suite, ...BOTH }, V13),
    { protocol: 'DTLSv1.3', cipher: suite });
  assert.deepStrictEqual(
    await handshake(BOTH, { ciphers: suite, ...V13 }),
    { protocol: 'DTLSv1.3', cipher: suite });
}

// A mixed string configures both versions.
{
  const ciphers = 'ECDHE-RSA-AES128-GCM-SHA256:TLS_CHACHA20_POLY1305_SHA256';
  assert.deepStrictEqual(
    await handshake({ ciphers, ...BOTH }, V13),
    { protocol: 'DTLSv1.3', cipher: 'TLS_CHACHA20_POLY1305_SHA256' });
  assert.deepStrictEqual(
    await handshake({ ciphers, ...BOTH }, V12),
    { protocol: 'DTLSv1.2', cipher: 'ECDHE-RSA-AES128-GCM-SHA256' });
}

// A DTLS 1.2 list leaves the default DTLS 1.3 suites in place, so it does
// not restrict a DTLS 1.3 handshake.
{
  const { protocol, cipher } = await handshake(
    { ciphers: 'ECDHE-RSA-AES128-GCM-SHA256', ...BOTH }, V13);
  assert.strictEqual(protocol, 'DTLSv1.3');
  assert.match(cipher, /^TLS_/);
}

// Naming only DTLS 1.3 suites raises the floor to DTLS 1.3: DTLS 1.2 would
// have nothing to negotiate.
assert.strictEqual(
  (await handshake({ ciphers: 'TLS_AES_128_GCM_SHA256', ...BOTH }, BOTH))
    .protocol,
  'DTLSv1.3');
await assert.rejects(
  handshake({ ciphers: 'TLS_AES_128_GCM_SHA256', ...BOTH }, V12),
  /protocol version/);

// ...and is refused outright where DTLS 1.3 is not enabled.
assert.throws(() => createSecureContext({
  ciphers: 'TLS_AES_128_GCM_SHA256', ...V12,
}), { code: 'ERR_INVALID_ARG_VALUE', message: /DTLS 1\.3 is not enabled/ });

// No DTLS 1.3 suite in common fails the handshake.
await assert.rejects(
  handshake({ ciphers: 'TLS_AES_128_GCM_SHA256', ...V13 },
            { ciphers: 'TLS_CHACHA20_POLY1305_SHA256', ...V13 }),
  /handshake failure/);

// Names OpenSSL does not know are rejected when the context is built.
assert.throws(() => listen(mustNotCall(), {
  cert, key, host: HOST, port: 0, ciphers: 'TLS_NOT_A_SUITE', ...V13,
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });
assert.throws(() => listen(mustNotCall(), {
  cert, key, host: HOST, port: 0, ciphers: '', ...V13,
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });
