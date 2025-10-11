// Flags: --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import { strictEqual, throws } from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const quic = await import('node:quic');

// Test that the main exports exist and are of the correct type.
strictEqual(typeof quic.connect, 'function');
strictEqual(typeof quic.listen, 'function');
strictEqual(typeof quic.QuicEndpoint, 'function');
strictEqual(typeof quic.QuicSession, 'function');
strictEqual(typeof quic.QuicStream, 'function');
strictEqual(typeof quic.QuicEndpoint.Stats, 'function');
strictEqual(typeof quic.QuicSession.Stats, 'function');
strictEqual(typeof quic.QuicStream.Stats, 'function');
strictEqual(typeof quic.constants, 'object');
strictEqual(typeof quic.constants.cc, 'object');

// Test that the constants exist and are of the correct type.
strictEqual(quic.constants.cc.RENO, 'reno');
strictEqual(quic.constants.cc.CUBIC, 'cubic');
strictEqual(quic.constants.cc.BBR, 'bbr');
strictEqual(quic.constants.DEFAULT_CIPHERS,
            'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:' +
            'TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256');
strictEqual(quic.constants.DEFAULT_GROUPS, 'X25519:P-256:P-384:P-521');

// Ensure the constants are.. well, constant.
throws(() => { quic.constants.cc.RENO = 'foo'; }, TypeError);
strictEqual(quic.constants.cc.RENO, 'reno');

throws(() => { quic.constants.cc.NEW_CONSTANT = 'bar'; }, TypeError);
strictEqual(quic.constants.cc.NEW_CONSTANT, undefined);

throws(() => { quic.constants.DEFAULT_CIPHERS = 123; }, TypeError);
strictEqual(typeof quic.constants.DEFAULT_CIPHERS, 'string');

throws(() => { quic.constants.NEW_CONSTANT = 456; }, TypeError);
strictEqual(quic.constants.NEW_CONSTANT, undefined);
