// Flags: --experimental-dtls --no-warnings

// Test: DTLS error handling for invalid certificate/key material and endpoint
// state.

import { hasCrypto, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, DTLSEndpoint } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const mismatchedKey = fixtures.readKey('agent2-key.pem').toString();

// Bad credentials report the reason OpenSSL gave, rather than a single
// ERR_CRYPTO_OPERATION_FAILED for every kind of failure. These codes come
// from OpenSSL and could shift if the bundled version changes; the point
// being pinned is that the three cases are distinguishable.

// A malformed certificate PEM is rejected.
assert.throws(() => listen(mustNotCall(), {
  cert: 'not a certificate', key, port: 0,
}), { code: 'ERR_OSSL_PEM_NO_START_LINE' });

// A malformed private key PEM is rejected.
assert.throws(() => listen(mustNotCall(), {
  cert, key: 'not a key', port: 0,
}), { code: 'ERR_OSSL_UNSUPPORTED' });

// A private key that does not match the certificate is rejected.
assert.throws(() => listen(mustNotCall(), {
  cert, key: mismatchedKey, port: 0,
}), { code: 'ERR_OSSL_X509_KEY_VALUES_MISMATCH' });

// Binding the same endpoint twice fails.
{
  const endpoint = new DTLSEndpoint();
  endpoint.bind('127.0.0.1', 0);
  assert.throws(() => endpoint.bind('127.0.0.1', 0), { code: 'ERR_INVALID_STATE' });
  await endpoint.close();
}
