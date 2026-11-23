// Flags: --experimental-quic --no-warnings

// Test: connect() rejects when the session closes during the first flight.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { connect } = await import('../common/quic.mjs');

// An empty list leaves the ClientHello without a cipher suite or a group, and
// a handshakeTimeout of 0 has already expired once the first flight is out.
const cases = [{ ciphers: '' }, { groups: '' }, { handshakeTimeout: 0 }];
for (const options of cases) {
  await assert.rejects(connect('127.0.0.1:1', options), {
    code: 'ERR_QUIC_CONNECTION_FAILED',
  });
}
