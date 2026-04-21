// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stateless reset.
// When the server loses session state and the client sends
//        data, the server sends a stateless reset and the client
//        session closes.
// When disableStatelessReset is true, the server does NOT
//        send a stateless reset.
// maxStatelessResetsPerHost rate limits the number of resets
//        sent to a single remote address.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual, rejects } = assert;

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
      ok(data.byteLength > 0);
      stream.writer.endSync();
      await stream.closed;

      // Now forcefully destroy the server session WITHOUT sending
      // CONNECTION_CLOSE. The client doesn't know the session
      // is gone.
      serverSession.destroy();
      serverDestroyed.resolve();
    });
  }), {
    onerror(err) { ok(err); },
  });

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    onerror: mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
    }),
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
  await rejects(clientSession.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });

  ok(serverEndpoint.stats.statelessResetCount > 0n,
     'Server should have sent a stateless reset');

  await serverEndpoint.close();
}

// disableStatelessReset prevents the server from sending resets.
{
  const serverDestroyed = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const data = await bytes(stream);
      ok(data.byteLength > 0);
      stream.writer.endSync();
      await stream.closed;

      serverSession.destroy();
      serverDestroyed.resolve();
    });
  }), {
    endpoint: { disableStatelessReset: true },
    onerror(err) { ok(err); },
  });

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    // Short idle timeout so the client doesn't hang waiting for
    // a stateless reset that will never arrive.
    transportParams: { maxIdleTimeout: 1 },
    // Onerror marks stream closed promises as handled so that the
    // idle-timeout stream destruction doesn't cause unhandled rejections.
    onerror(err) { ok(err); },
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

  strictEqual(serverEndpoint.stats.statelessResetCount, 0n,
              'No stateless reset should have been sent');

  await serverEndpoint.close();
}

// maxStatelessResetsPerHost rate limits resets per remote address.
// The LRU tracks resets per IP+port, so both sessions must share a
// client endpoint to have the same source address.
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
      ok(data.byteLength > 0);
      stream.writer.endSync();
      await stream.closed;

      serverSession.destroy();
      deferred.resolve();
    });
  }, 2), {
    endpoint: { maxStatelessResetsPerHost: 1 },
    onerror(err) { ok(err); },
  });

  // Both clients share an endpoint so the server sees the same
  // remote IP+port for both, making the rate limiter apply.
  const { QuicEndpoint } = await import('node:quic');
  const clientEndpoint = new QuicEndpoint();

  // --- First session: triggers a stateless reset ---

  const client1 = await connect(serverEndpoint.address, {
    endpoint: clientEndpoint,
    onerror: mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
    }),
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
  await rejects(client1.closed, { code: 'ERR_QUIC_TRANSPORT_ERROR' });

  strictEqual(serverEndpoint.stats.statelessResetCount, 1n,
              'First reset should have been sent');

  // --- Second session: rate-limited, no reset sent ---

  const client2 = await connect(serverEndpoint.address, {
    endpoint: clientEndpoint,
    // Short idle timeout so the client closes after the server
    // destroys (no stateless reset will arrive, rate-limited).
    transportParams: { maxIdleTimeout: 1 },
    // Onerror marks stream closed promises as handled.
    onerror(err) { ok(err); },
  });
  await client2.opened;

  // Send data so the server onstream fires and destroys the session.
  await client2.createBidirectionalStream({
    body: encoder.encode('session2'),
  });
  await serverDestroyed2.promise;

  // Send a packet — the server would normally send a stateless reset,
  // but the rate limit (1 per host) is already exhausted.
  // eslint-disable-next-line no-unused-vars
  const s2b = await client2.createBidirectionalStream({
    body: encoder.encode('after destroy 2'),
  });

  // The client closes via idle timeout (no stateless reset).
  await client2.closed;

  strictEqual(serverEndpoint.stats.statelessResetCount, 1n,
              'Second reset should have been rate-limited');

  await clientEndpoint.close();
  await serverEndpoint.close();
}
