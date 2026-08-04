// Flags: --experimental-quic --no-warnings

// Regression test: HTTP/3 server must not crash when a session is closed
// before the H3 application is fully started (control streams bound).
// Previously, closing such a session would call nghttp3_conn_shutdown on
// an H3 connection whose control streams were never bound, causing an
// assertion failure in nghttp3 (conn->tx.ctrl != NULL).
//
// The test creates an H3 server and a client that immediately closes the
// session before the handshake completes. The server creates the H3
// application during ALPN negotiation, but Start() (which binds control
// streams) hasn't been called yet when the session is torn down.
// The server must handle this gracefully without crashing.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import { setTimeout } from 'node:timers/promises';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustNotCall(),
});

// Connect then immediately close the session before the handshake completes.
// This exercises the H3 shutdown path on the server while the H3 application
// exists but hasn't started (control streams not yet bound).
const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  // h3 ALPN — must match the server so the H3 application is selected
  // on the server side before we tear it down.
});

// Close immediately — don't wait for handshake.
await clientSession.close();

// Give the server time to process the close and tear down the session.
await setTimeout(500);

// The critical assertion: reaching this point without a crash means the
// server correctly handled the H3 shutdown before control streams were
// bound. Verify the endpoint is still alive by closing it gracefully.
await serverEndpoint.close();
