// Flags: --experimental-quic --no-warnings

// Test: endpoint idle timeout behavior.
// When an endpoint has idleTimeout > 0 and becomes idle (no sessions,
// not listening), it stays alive for the timeout duration before
// being destroyed. With idleTimeout = 0 (default), the endpoint is
// destroyed immediately when idle.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { QuicEndpoint } = await import('node:quic');
const { listen, connect } = await import('../common/quic.mjs');

// --- Test 1: Default idleTimeout (0) --- endpoint becomes idle
// immediately when it has no sessions and is not listening. The
// UDP handle is unref'd so it won't block process exit.
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }));

  // Create a client with an explicit endpoint so we can track it.
  const clientEndpoint = new QuicEndpoint();
  const client = await connect(serverEndpoint.address, {
    endpoint: clientEndpoint,
  });
  await client.opened;

  // Endpoint is alive while the session is active.
  ok(!clientEndpoint.destroyed, 'endpoint should be alive');

  await client.close();

  // The endpoint's UDP handle is unref'd when all sessions close,
  // so it won't block process exit. Explicitly close it for cleanup.
  await clientEndpoint.close();
  ok(clientEndpoint.destroyed, 'endpoint should be destroyed after close');

  await serverEndpoint.close();
}

// --- Test 2: idleTimeout > 0 --- endpoint stays alive briefly
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }));

  // Create endpoint with a 1-second idle timeout.
  const clientEndpoint = new QuicEndpoint({ idleTimeout: 1 });
  const client = await connect(serverEndpoint.address, {
    endpoint: clientEndpoint,
  });
  await client.opened;
  await client.close();

  // The endpoint should NOT be immediately destroyed — idle timer
  // is running.
  ok(!clientEndpoint.destroyed,
     'endpoint should still be alive during idle timeout');

  // Wait for the idle timeout to fire (1 second + margin).
  // Use a ref'd timer to keep the event loop alive while the
  // unref'd idle timer runs.
  await setTimeout(2000);
  ok(clientEndpoint.destroyed,
     'endpoint should be destroyed after idle timeout');

  await serverEndpoint.close();
}
