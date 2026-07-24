// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stateless reset.
// When the server loses session state and the client sends
//        data, the server sends a stateless reset and the client
//        session closes.
// When disableStatelessReset is true, the server does NOT
//        send a stateless reset.
// Global token bucket rate limits the total number of resets.

import { hasQuic, skip, mustCall, expectsError } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

// Stateless reset received closes session.
{
  const serverDestroyed = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      // Do a complete data exchange first so both sides are
      // fully at 1-RTT with all ACKs exchanged.
      const data = await bytes(stream);
      assert.ok(data.byteLength > 0);
      stream.writer.endSync();
      await stream.closed;

      // Now forcefully destroy the server session WITHOUT sending
      // CONNECTION_CLOSE. The client doesn't know the session
      // is gone.
      serverSession.destroy();
      serverDestroyed.resolve();
    });
  }), {
    onerror: expectsError(),
  });

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    verifyPeer: 'manual',
    onerror: expectsError({ code: 'ERR_QUIC_TRANSPORT_ERROR' }),
  });
  await clientSession.opened;

  // First exchange: complete round-trip to confirm 1-RTT.
  const stream1 = await clientSession.createBidirectionalStream({
    body: encoder.encode('hello'),
  });
  for await (const _ of stream1) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream1.closed;

  // Wait for the server to destroy.
  await serverDestroyed.promise;

  // Open a second stream — this sends a short header (1-RTT) packet
  // to the server. The server endpoint doesn't recognize the DCID
  // and should send a stateless reset.
  // eslint-disable-next-line no-unused-vars
  const stream2 = await clientSession.createBidirectionalStream({
    body: encoder.encode('after destroy'),
  });

  // The client session should be closed by the stateless reset.
  await assert.rejects(clientSession.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });

  assert.ok(serverEndpoint.stats.statelessResetCount > 0n,
            'Server should have sent a stateless reset');

  await serverEndpoint.close();
}

// disableStatelessReset prevents the server from sending resets.
{
  const serverDestroyed = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const data = await bytes(stream);
      assert.ok(data.byteLength > 0);
      stream.writer.endSync();
      await stream.closed;

      serverSession.destroy();
      serverDestroyed.resolve();
    });
  }), {
    endpoint: { disableStatelessReset: true },
    onerror: expectsError(),
  });

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    verifyPeer: 'manual',
    // Short idle timeout so the client doesn't hang waiting for
    // a stateless reset that will never arrive.
    transportParams: { maxIdleTimeout: 1 },
    // Onerror marks stream closed promises as handled so that the
    // idle-timeout stream destruction doesn't cause unhandled rejections.
    onerror: expectsError(),
  });
  await clientSession.opened;

  const stream1 = await clientSession.createBidirectionalStream({
    body: encoder.encode('hello'),
  });
  for await (const _ of stream1) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream1.closed;

  await serverDestroyed.promise;

  // Send a packet after the server session is destroyed. The server
  // endpoint silently drops the packet (stateless reset disabled).
  // eslint-disable-next-line no-unused-vars
  const stream2 = await clientSession.createBidirectionalStream({
    body: encoder.encode('after destroy'),
  });

  // The client should NOT receive a stateless reset. It will close
  // via idle timeout instead.
  await clientSession.closed;

  assert.strictEqual(serverEndpoint.stats.statelessResetCount, 0n); // No stateless reset should have been sent

  await serverEndpoint.close();
}

// Global token bucket rate limits stateless resets.
// With burst=1 and rate=0, only one reset can be sent.
{
  let sessionCount = 0;
  const serverDestroyed1 = Promise.withResolvers();
  const serverDestroyed2 = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    sessionCount++;
    const which = sessionCount;
    const deferred = which === 1 ? serverDestroyed1 : serverDestroyed2;

    serverSession.onstream = mustCall(async (stream) => {
      const data = await bytes(stream);
      assert.ok(data.byteLength > 0);
      stream.writer.endSync();
      await stream.closed;

      serverSession.destroy();
      deferred.resolve();
    });
  }, 2), {
    endpoint: { statelessResetBurst: 1, statelessResetRate: 0 },
    onerror: expectsError(),
  });

  // The global token bucket rate limiter applies regardless of
  // client source address.
  const { QuicEndpoint } = await import('node:quic');
  const clientEndpoint = new QuicEndpoint();

  // --- First session: triggers a stateless reset ---

  const client1 = await connect(serverEndpoint.address, {
    endpoint: clientEndpoint,
    verifyPeer: 'manual',
    onerror: expectsError({ code: 'ERR_QUIC_TRANSPORT_ERROR' }),
  });
  await client1.opened;

  // Send data so the server onstream fires and destroys the session.
  await client1.createBidirectionalStream({
    body: encoder.encode('session1'),
  });
  await serverDestroyed1.promise;

  // Send a packet to trigger stateless reset.
  // eslint-disable-next-line no-unused-vars
  const s1b = await client1.createBidirectionalStream({
    body: encoder.encode('after destroy 1'),
  });
  await assert.rejects(client1.closed, { code: 'ERR_QUIC_TRANSPORT_ERROR' });

  assert.strictEqual(serverEndpoint.stats.statelessResetCount, 1n); // First reset should have been sent

  // --- Second session: rate-limited, no reset sent ---

  const client2 = await connect(serverEndpoint.address, {
    endpoint: clientEndpoint,
    verifyPeer: 'manual',
    // Short idle timeout so the client closes after the server
    // destroys (no stateless reset will arrive, rate-limited).
    transportParams: { maxIdleTimeout: 1 },
    // Onerror marks stream closed promises as handled.
    onerror: expectsError(),
  });
  await client2.opened;

  // Send data so the server onstream fires and destroys the session.
  await client2.createBidirectionalStream({
    body: encoder.encode('session2'),
  });
  await serverDestroyed2.promise;

  // Send a packet — the server would normally send a stateless reset,
  // but the global rate limit (burst of 1) is already exhausted.
  // eslint-disable-next-line no-unused-vars
  const s2b = await client2.createBidirectionalStream({
    body: encoder.encode('after destroy 2'),
  });

  // The client closes via idle timeout (no stateless reset).
  await client2.closed;

  assert.strictEqual(serverEndpoint.stats.statelessResetCount, 1n); // Second reset should have been rate-limited
  assert.ok(serverEndpoint.stats.statelessResetRateLimited > 0); // Rate-limited counter should be non-zero

  await clientEndpoint.close();
  await serverEndpoint.close();
}
