// Flags: --experimental-dtls --no-warnings

// Test: DTLS error handling for invalid certificate/key material and endpoint
// state.

import { hasCrypto, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { throws } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, DTLSEndpoint } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const mismatchedKey = readKey('agent2-key.pem').toString();

// A malformed certificate PEM is rejected.
throws(() => listen(mustNotCall(), {
  cert: 'not a certificate', key, port: 0,
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });

// A malformed private key PEM is rejected.
throws(() => listen(mustNotCall(), {
  cert, key: 'not a key', port: 0,
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });

// A private key that does not match the certificate is rejected.
throws(() => listen(mustNotCall(), {
  cert, key: mismatchedKey, port: 0,
}), { code: 'ERR_CRYPTO_OPERATION_FAILED' });

// Binding the same endpoint twice fails.
{
  const endpoint = new DTLSEndpoint();
  endpoint.bind('127.0.0.1', 0);
  throws(() => endpoint.bind('127.0.0.1', 0), { code: 'ERR_INVALID_STATE' });
  await endpoint.close();
}
