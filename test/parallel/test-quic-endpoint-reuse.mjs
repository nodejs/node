// Flags: --experimental-quic --no-warnings

// Test: endpoint reuse behavior for connect().
// 1. Multiple connect() calls without explicit endpoint share
//    the same endpoint (connection pooling).
// 2. connect() with reuseEndpoint: false creates a separate endpoint.
// 3. connect() to the same address as a listening endpoint does NOT
//    reuse the listening endpoint (self-connect exclusion).
// 4. connect() to a different address with a listening endpoint in
//    the registry reuses the listening endpoint (dual-role).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, notStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// --- Test 1: connect() reuses endpoints by default ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }, 2));

  const client1 = await connect(serverEndpoint.address);
  await client1.opened;

  const client2 = await connect(serverEndpoint.address);
  await client2.opened;

  // Both client sessions should share the same endpoint because
  // findSuitableEndpoint returns the first available non-listening
  // non-closing endpoint. After client1 is created, its endpoint
  // is available for client2.
  strictEqual(client1.endpoint, client2.endpoint,
              'client sessions should share an endpoint');

  await client1.close();
  await client2.close();
  await serverEndpoint.close();
}

// --- Test 2: reuseEndpoint: false creates separate endpoints ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }, 2));

  const client1 = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await client1.opened;

  const client2 = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await client2.opened;

  notStrictEqual(client1.endpoint, client2.endpoint,
                 'client sessions should have separate endpoints');

  await client1.close();
  await client2.close();
  await serverEndpoint.close();
}

// --- Test 3: connect() to a listening endpoint's address is not reused ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }));

  const client = await connect(serverEndpoint.address);
  await client.opened;

  // The client endpoint should NOT be the server endpoint, even though
  // the server endpoint is in the registry. Self-connect is excluded
  // because the client's DCID associations would collide with the
  // server's session routing on the same endpoint.
  notStrictEqual(client.endpoint, serverEndpoint,
                 'client should not reuse the server endpoint');

  await client.close();
  await serverEndpoint.close();
}
