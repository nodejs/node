// Flags: --experimental-dtls --no-warnings

// Test: Option validation for DTLS API.

import { hasCrypto, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { throws } = assert;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

// Test: listen() requires a callback.
throws(() => {
  listen(undefined, { cert: 'x', key: 'y', port: 0 });
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: listen() requires cert.
throws(() => {
  listen(mustNotCall(), { key: 'y', port: 0 });
}, { code: 'ERR_MISSING_ARGS' });

// Test: listen() requires key.
throws(() => {
  listen(mustNotCall(), { cert: 'x', port: 0 });
}, { code: 'ERR_MISSING_ARGS' });

// Test: listen() requires port.
throws(() => {
  listen(mustNotCall(), { cert: 'x', key: 'y' });
}, { code: 'ERR_MISSING_ARGS' });

// Test: connect() requires valid host.
throws(() => {
  connect(123, 4433);
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: connect() requires valid port.
throws(() => {
  connect('localhost', 'invalid');
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: connect() rejects out-of-range port.
throws(() => {
  connect('localhost', 99999);
}, { code: 'ERR_OUT_OF_RANGE' });

// Test: mtu must be an integer within [256, 65535].
throws(() => {
  connect('127.0.0.1', 4433, { mtu: 100 });
}, { code: 'ERR_OUT_OF_RANGE' });

throws(() => {
  connect('127.0.0.1', 4433, { mtu: 70000 });
}, { code: 'ERR_OUT_OF_RANGE' });

// Test: alpn must be a string array or Buffer.
throws(() => {
  connect('127.0.0.1', 4433, { alpn: 123 });
}, { code: 'ERR_INVALID_ARG_TYPE' });
