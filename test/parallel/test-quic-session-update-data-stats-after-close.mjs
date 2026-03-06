// Flags: --experimental-quic --no-warnings
// Regression test for https://github.com/nodejs/node/pull/62126
// UpdateDataStats() must not crash when called after the session's impl_ has
// been reset to null (i.e. after the session is destroyed).
//
// The crash path is in Session::SendDatagram():
//   auto on_exit = OnScopeLeave([&] { ...; UpdateDataStats(); });
// If the send encounters an error it calls Close(SILENT) → Destroy() →
// impl_.reset(). The on_exit lambda then fires with impl_ == nullptr.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const certs = fixtures.readKey('agent1-cert.pem');

// Datagrams must be enabled on both sides for sendDatagram() to work.
const kDatagramOptions = {
  transportParams: {
    maxDatagramFrameSize: 1200n,
  },
};

const serverEndpoint = await listen(
  mustCall((serverSession) => {
    serverSession.opened.then(mustCall(async () => {
      // Send a datagram then immediately close. This exercises the
      // UpdateDataStats() call that fires via on_exit after SendDatagram —
      // the close can race with or precede the stats update, leaving
      // impl_ == nullptr. Before the fix this would crash.
      serverSession.sendDatagram(Buffer.from('hello')).catch(() => {});
      serverSession.close();
    }));
  }),
  { keys, certs, ...kDatagramOptions },
);

const clientSession = await connect(serverEndpoint.address, kDatagramOptions);
await clientSession.opened;

// Mirror the race on the client side.
clientSession.sendDatagram(Buffer.from('world')).catch(() => {});
clientSession.close();

await clientSession.closed;
serverEndpoint.close();
await serverEndpoint.closed;
