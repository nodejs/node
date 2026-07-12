// Flags: --experimental-quic --no-warnings

// Test: endpoint block list filtering.
// Deny mode: packets from blocked addresses are dropped.
// Allow mode: only packets from listed addresses are accepted.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import net from 'node:net';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// --- Deny mode: block 127.0.0.1 ---
{
  const blockList = new net.BlockList();
  blockList.addAddress('127.0.0.1');

  const serverEndpoint = await listen(mustNotCall(), {
    endpoint: {
      blockList,
      blockListPolicy: 'deny',
    },
  });

  // Connecting from 127.0.0.1 should be silently dropped.
  // The client will time out waiting for a response.
  const clientSession = await connect(serverEndpoint.address, {
    handshakeTimeout: 500,
    verifyPeer: 'manual',
    onerror: mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
    }),
  });

  // The session should fail — the server never sees the packet.
  await assert.rejects(clientSession.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });

  // Verify the stat counter.
  assert.ok(serverEndpoint.stats.packetsBlocked > 0n,
            'packetsBlocked should be non-zero');

  await serverEndpoint.close();
}

// --- Allow mode: only allow 127.0.0.1 ---
{
  const allowList = new net.BlockList();
  allowList.addAddress('127.0.0.1');

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    serverSession.close();
  }), {
    endpoint: {
      blockList: allowList,
      blockListPolicy: 'allow',
    },
  });

  // Connecting from 127.0.0.1 should succeed — it's in the allow list.
  const clientSession = await connect(serverEndpoint.address, {
    verifyPeer: 'manual',
  });
  await clientSession.opened;
  await clientSession.close();

  assert.strictEqual(serverEndpoint.stats.packetsBlocked, 0n); // No packets should be blocked for allowed address

  await serverEndpoint.close();
}
