// Flags: --experimental-quic --no-warnings

// Test: ALPN mismatch causes connection failure.
// The server offers 'quic-test' but the client requests 'nonexistent'.
// The handshake should fail with a `no_application_protocol` alert.
//
// The QUIC transport error code for a CRYPTO_ERROR carrying a TLS alert is
// 0x100 | <tls_alert>. For `no_application_protocol` (alert 120 / 0x78) this
// is 0x178 == 376.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual, match } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const expected = {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
  errorCode: 376n,
  message: /no application protocol/
};

const onerror = mustCall((err) => {
  strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  strictEqual(err.errorCode, 376n);
  match(err.message, /no application protocol/);
}, 2);
const transportParams = { maxIdleTimeout: 1 };

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await rejects(serverSession.opened, expected);
  await rejects(serverSession.closed, expected);
}), {
  transportParams,
  onerror,
});

// Client requests an ALPN the server doesn't offer.
const clientSession = await connect(serverEndpoint.address, {
  alpn: 'nonexistent-protocol',
  transportParams,
  onerror,
});

await rejects(clientSession.opened, expected);

// The handshake should fail — opened may reject or never resolve.
// The session should close with an error.
await rejects(clientSession.closed, expected);
await serverEndpoint.close();
