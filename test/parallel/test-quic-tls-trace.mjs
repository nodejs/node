// Flags: --experimental-quic --no-warnings

// Test: TLS trace output.
// When tlsTrace: true is set, the session produces TLS debug
// output on stderr. Verify the option is accepted without error
// and the connection succeeds.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  await serverSession.close();
}), {
  tlsTrace: true,
});

const clientSession = await connect(serverEndpoint.address, {
  tlsTrace: true,
});
await clientSession.opened;
strictEqual(clientSession.destroyed, false);

await clientSession.closed;
await serverEndpoint.close();
