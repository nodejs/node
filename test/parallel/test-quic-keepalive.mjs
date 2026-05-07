// Flags: --experimental-quic --no-warnings

// Test: keepAlive option.
// keepAlive keeps idle connection alive past default timeout.
// keepAlive: 0 (default) does not send PING frames.
// Keep-alive PING frames visible in session stats (pingRecv).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// KA-01/03: With keepAlive set, the connection stays alive and
// PING frames are sent. After a brief idle period, the peer's
// pingRecv stat should be > 0.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    // Wait for keep-alive PINGs to arrive.
    await setTimeout(300);
    // Server should have received PING frames.
    ok(serverSession.stats.pingRecv > 0n,
       'Server should receive keep-alive PINGs');
    serverSession.close();
    serverDone.resolve();
  }), {
    transportParams: { maxIdleTimeout: 10 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    keepAlive: 100,  // Send PING every 100ms.
    transportParams: { maxIdleTimeout: 10 },
  });

  await Promise.all([clientSession.opened, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// Without keepAlive (default), no additional PINGs after handshake.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    // Record PINGs from handshake.
    const handshakePings = serverSession.stats.pingRecv;
    await setTimeout(200);
    // No additional PINGs should arrive without keepAlive.
    strictEqual(serverSession.stats.pingRecv, handshakePings);
    serverSession.close();
    serverDone.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  await Promise.all([serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}
