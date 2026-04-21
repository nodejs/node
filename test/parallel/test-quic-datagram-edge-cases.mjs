// Flags: --experimental-quic --no-warnings

// Test: datagram edge cases.
// DGRAM-08 / DGIMP-08: Zero-length datagram returns 0n (not sent).
// DGC-01 / DGIMP-09: maxDatagramFrameSize: 0 disables datagrams entirely.
// Datagram arrives with no ondatagram callback — no crash.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';

const { strictEqual, notStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// --- DGRAM-08 / DGIMP-08: Zero-length datagram returns 0n ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }), {
    transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Zero-length ArrayBufferView
  const zeroId = await clientSession.sendDatagram(new Uint8Array(0));
  strictEqual(zeroId, 0n);

  // Zero-length string
  const emptyStringId = await clientSession.sendDatagram('');
  strictEqual(emptyStringId, 0n);

  await clientSession.close();
  await serverEndpoint.close();
}

// --- DGC-01 / DGIMP-09: maxDatagramFrameSize: 0 disables datagrams ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }), {
    // Server advertises 0 — no datagrams accepted.
    transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 0 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 10 },
  });
  await clientSession.opened;

  // maxDatagramSize reflects the peer's (server's) transport param.
  strictEqual(clientSession.maxDatagramSize, 0);

  // Sending returns 0n immediately — datagram not sent.
  const id = await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));
  strictEqual(id, 0n);

  await clientSession.close();
  await serverEndpoint.close();
}

// --- DGRAM-11: No ondatagram callback — no crash ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    // No ondatagram set — datagrams arrive but are silently discarded.
    await serverSession.opened;
    // Give time for the datagram to arrive and be processed without crash.
    await setTimeout(200);
    await serverSession.close();
  }), {
    transportParams: { maxDatagramFrameSize: 1200 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Send a datagram even though the server has no ondatagram handler.
  const id = await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));
  notStrictEqual(id, 0n);

  await clientSession.closed;
  await serverEndpoint.close();
}
