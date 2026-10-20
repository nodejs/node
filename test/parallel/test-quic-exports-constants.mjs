// Flags: --experimental-quic --no-warnings

// Test: node:quic exports and constants.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const quic = await import('node:quic');

// Top-level exports.
assert.strictEqual(typeof quic.listen, 'function');
assert.strictEqual(typeof quic.connect, 'function');
assert.strictEqual(typeof quic.QuicEndpoint, 'function');
assert.strictEqual(typeof quic.QuicSession, 'function');
assert.strictEqual(typeof quic.QuicStream, 'function');
assert.strictEqual(typeof quic.constants, 'object');

// Congestion control constants.
assert.strictEqual(quic.constants.cc.RENO, 'reno');
assert.strictEqual(quic.constants.cc.CUBIC, 'cubic');
assert.strictEqual(quic.constants.cc.BBR, 'bbr');

// DEFAULT_CIPHERS.
assert.strictEqual(typeof quic.constants.DEFAULT_CIPHERS, 'string');
assert.ok(quic.constants.DEFAULT_CIPHERS.length > 0);
assert.ok(quic.constants.DEFAULT_CIPHERS.includes('TLS_AES_128_GCM_SHA256'));

// DEFAULT_GROUPS.
assert.strictEqual(typeof quic.constants.DEFAULT_GROUPS, 'string');
assert.ok(quic.constants.DEFAULT_GROUPS.length > 0);

// QuicEndpoint can be constructed directly.
// QuicSession and QuicStream cannot — they throw ERR_ILLEGAL_CONSTRUCTOR.
{
  const ep = new quic.QuicEndpoint();
  assert.ok(ep instanceof quic.QuicEndpoint);
}
assert.throws(() => new quic.QuicSession(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});
assert.throws(() => new quic.QuicStream(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});
