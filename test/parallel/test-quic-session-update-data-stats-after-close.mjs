// Flags: --experimental-quic --no-warnings
// Regression test for https://github.com/nodejs/node/pull/62126
//
// Validates that UpdateDataStats() does not crash when called on a session
// whose impl_ has already been reset to null (i.e. after Destroy()).
//
// The crash path in Session::SendDatagram():
//   auto on_exit = OnScopeLeave([&] { ...; UpdateDataStats(); });
// When an internal send error calls Close(SILENT) -> Destroy() -> impl_.reset(),
// the on_exit lambda fires with impl_ == nullptr.  The `if (!impl_) return;`
// guard added by this PR prevents the crash.
//
// Note: the precise crash trigger (a packet-allocation failure inside
// SendDatagram) cannot be induced from JS.  This test exercises the closest
// reachable scenario: datagrams sent immediately before session close, racing
// the on_exit UpdateDataStats() call against teardown.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const certs = fixtures.readKey('agent1-cert.pem');

// Both sides must negotiate datagram support (maxDatagramFrameSize > 0).
const kDatagramOptions = {
  transportParams: {
    maxDatagramFrameSize: 1200n,
  },
};

const serverClosed = Promise.withResolvers();

const serverEndpoint = await listen(
  mustCall((serverSession) => {
    serverSession.opened.then(mustCall(async () => {
      // Fire a datagram send then close immediately.  This races the
      // UpdateDataStats() invoked by on_exit in SendDatagram() against the
      // Destroy() triggered by close(), exercising the null-impl_ guard.
      serverSession.sendDatagram(Buffer.from('hello')).catch(() => {});
      serverSession.close();
    }));
    serverSession.closed.then(mustCall(() => serverClosed.resolve()));
  }),
  { keys, certs, ...kDatagramOptions },
);

// Pass the server certificate as the CA so TLS validation succeeds on the
// client side (agent1-cert.pem is self-signed).
const clientSession = await connect(serverEndpoint.address, {
  ca: certs,
  ...kDatagramOptions,
});

await clientSession.opened;

// Mirror the race on the client side.
clientSession.sendDatagram(Buffer.from('world')).catch(() => {});
clientSession.close();

await clientSession.closed;
await serverClosed.promise;
serverEndpoint.close();
await serverEndpoint.closed;
