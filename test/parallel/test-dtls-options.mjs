// Flags: --experimental-dtls --no-warnings

// Test: Option validation for DTLS API.

import { hasCrypto, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

// Test: listen() requires a callback.
assert.throws(() => {
  listen(undefined, { cert: 'x', key: 'y', port: 0 });
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: listen() requires cert.
assert.throws(() => {
  listen(mustNotCall(), { key: 'y', port: 0 });
}, { code: 'ERR_MISSING_ARGS' });

// Test: listen() requires key.
assert.throws(() => {
  listen(mustNotCall(), { cert: 'x', port: 0 });
}, { code: 'ERR_MISSING_ARGS' });

// Test: listen() requires port.
assert.throws(() => {
  listen(mustNotCall(), { cert: 'x', key: 'y' });
}, { code: 'ERR_MISSING_ARGS' });

// Test: connect() requires valid host.
assert.throws(() => {
  connect(123, 4433);
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: connect() requires valid port.
assert.throws(() => {
  connect('localhost', 'invalid');
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: connect() rejects out-of-range port.
assert.throws(() => {
  connect('localhost', 99999);
}, { code: 'ERR_OUT_OF_RANGE' });
