// Flags: --experimental-quic --no-warnings

// Test: node:quic exports and constants.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, throws, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const quic = await import('node:quic');

// Top-level exports.
strictEqual(typeof quic.listen, 'function');
strictEqual(typeof quic.connect, 'function');
strictEqual(typeof quic.QuicEndpoint, 'function');
strictEqual(typeof quic.QuicSession, 'function');
strictEqual(typeof quic.QuicStream, 'function');
strictEqual(typeof quic.constants, 'object');

// Congestion control constants.
strictEqual(quic.constants.cc.RENO, 'reno');
strictEqual(quic.constants.cc.CUBIC, 'cubic');
strictEqual(quic.constants.cc.BBR, 'bbr');

// DEFAULT_CIPHERS.
strictEqual(typeof quic.constants.DEFAULT_CIPHERS, 'string');
ok(quic.constants.DEFAULT_CIPHERS.length > 0);
ok(quic.constants.DEFAULT_CIPHERS.includes('TLS_AES_128_GCM_SHA256'));

// DEFAULT_GROUPS.
strictEqual(typeof quic.constants.DEFAULT_GROUPS, 'string');
ok(quic.constants.DEFAULT_GROUPS.length > 0);

// QuicEndpoint can be constructed directly.
// QuicSession and QuicStream cannot — they throw ERR_ILLEGAL_CONSTRUCTOR.
{
  const ep = new quic.QuicEndpoint();
  ok(ep instanceof quic.QuicEndpoint);
}
throws(() => new quic.QuicSession(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});
throws(() => new quic.QuicStream(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});
