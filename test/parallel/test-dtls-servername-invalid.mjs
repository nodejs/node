// Flags: --experimental-dtls --no-warnings

// Test: connect() surfaces a failure to set the SNI servername (for example a
// name longer than the TLS maximum of 255 bytes) as an error, rather than
// silently ignoring it.

import { hasCrypto, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect } = await import('node:dtls');

// SNI host names are limited to 255 bytes; a longer one must throw.
assert.throws(
  () => connect('127.0.0.1', 12345, {
    servername: 'a'.repeat(256),
    rejectUnauthorized: false,
  }),
  { code: 'ERR_CRYPTO_OPERATION_FAILED' },
);
