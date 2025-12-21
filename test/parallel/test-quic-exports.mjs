// Flags: --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const quic = await import('node:quic');

// Test that the main exports exist and are of the correct type.
assert.strictEqual(typeof quic.connect, 'function');
assert.strictEqual(typeof quic.listen, 'function');
assert.strictEqual(typeof quic.QuicEndpoint, 'function');
assert.strictEqual(typeof quic.QuicSession, 'function');
assert.strictEqual(typeof quic.QuicStream, 'function');
assert.strictEqual(typeof quic.QuicEndpoint.Stats, 'function');
assert.strictEqual(typeof quic.QuicSession.Stats, 'function');
assert.strictEqual(typeof quic.QuicStream.Stats, 'function');
assert.strictEqual(typeof quic.constants, 'object');
assert.strictEqual(typeof quic.constants.cc, 'object');

// Test that the constants exist and are of the correct type.
assert.strictEqual(quic.constants.cc.RENO, 'reno');
assert.strictEqual(quic.constants.cc.CUBIC, 'cubic');
assert.strictEqual(quic.constants.cc.BBR, 'bbr');
assert.strictEqual(quic.constants.DEFAULT_CIPHERS,
                   'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:' +
            'TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256');
assert.strictEqual(quic.constants.DEFAULT_GROUPS, 'X25519:P-256:P-384:P-521');

// Ensure the constants are.. well, constant.
assert.throws(() => { quic.constants.cc.RENO = 'foo'; }, TypeError);
assert.strictEqual(quic.constants.cc.RENO, 'reno');

assert.throws(() => { quic.constants.cc.NEW_CONSTANT = 'bar'; }, TypeError);
assert.strictEqual(quic.constants.cc.NEW_CONSTANT, undefined);

assert.throws(() => { quic.constants.DEFAULT_CIPHERS = 123; }, TypeError);
assert.strictEqual(typeof quic.constants.DEFAULT_CIPHERS, 'string');

assert.throws(() => { quic.constants.NEW_CONSTANT = 456; }, TypeError);
assert.strictEqual(quic.constants.NEW_CONSTANT, undefined);
