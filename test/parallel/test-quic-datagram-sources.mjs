// Flags: --experimental-quic --no-warnings

// Test: sendDatagram with various input source types.
// String with custom encoding (e.g., 'hex').
// Promise input — resolves then sends.
// Promise input — session closes during await, returns 0n.
// SharedArrayBuffer copies instead of transfers.
// Pooled Buffer (partial view) copies correctly.
// DataView input.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual, notStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// --- DGIMP-01: String with custom encoding ---
{
  const received = [];
  const allReceived = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await allReceived.promise;
    await serverSession.close();
  }), {
    transportParams: { maxDatagramFrameSize: 1200 },
    ondatagram: mustCall((data) => {
      received.push(Buffer.from(data));
      if (received.length === 2) allReceived.resolve();
    }, 2),
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Send hex-encoded string — '48656c6c6f' is 'Hello' in hex.
  const hexId = await clientSession.sendDatagram('48656c6c6f', 'hex');
  notStrictEqual(hexId, 0n);

  // Send base64-encoded string — 'V29ybGQ=' is 'World' in base64.
  const b64Id = await clientSession.sendDatagram('V29ybGQ=', 'base64');
  notStrictEqual(b64Id, 0n);

  await allReceived.promise;

  deepStrictEqual(received[0], Buffer.from('Hello'));
  deepStrictEqual(received[1], Buffer.from('World'));

  await clientSession.closed;
  await serverEndpoint.close();
}

// --- DGIMP-02: Promise input ---
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverGot.promise;
    await serverSession.close();
  }), {
    transportParams: { maxDatagramFrameSize: 1200 },
    ondatagram: mustCall((data) => {
      deepStrictEqual(Buffer.from(data), Buffer.from([42]));
      serverGot.resolve();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Send a Promise that resolves to a Uint8Array.
  const promiseId = await clientSession.sendDatagram(
    Promise.resolve(new Uint8Array([42])),
  );
  notStrictEqual(promiseId, 0n);

  await Promise.all([serverGot.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// --- DGIMP-03: Promise input, session closes during await ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }), {
    transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
    onerror() {},
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Create a promise that resolves after the session starts closing.
  // sendDatagram passes the initial checks, then awaits the promise.
  // While awaiting, the session closes. When the promise resolves,
  // sendDatagram finds the session closed and returns 0n.
  const slowPromise = new Promise((resolve) => {
    setImmediate(mustCall(async () => {
      await clientSession.close();
      resolve(new Uint8Array([1]));
    }));
  });

  const id = await clientSession.sendDatagram(slowPromise);
  strictEqual(id, 0n);

  await serverEndpoint.close();
}

// --- DGIMP-04: SharedArrayBuffer ---
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverGot.promise;
    await serverSession.close();
  }), {
    transportParams: { maxDatagramFrameSize: 1200 },
    ondatagram: mustCall((data) => {
      deepStrictEqual(Buffer.from(data), Buffer.from([10, 20, 30]));
      serverGot.resolve();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Create a SharedArrayBuffer-backed view.
  const sab = new SharedArrayBuffer(3);
  const view = new Uint8Array(sab);
  view[0] = 10;
  view[1] = 20;
  view[2] = 30;

  const id = await clientSession.sendDatagram(view);
  notStrictEqual(id, 0n);

  // The SharedArrayBuffer should still be usable (copied, not transferred).
  strictEqual(view[0], 10);

  await Promise.all([serverGot.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// --- DGIMP-05: Pooled Buffer (partial view) ---
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverGot.promise;
    await serverSession.close();
  }), {
    transportParams: { maxDatagramFrameSize: 1200 },
    ondatagram: mustCall((data) => {
      // The received data should match the slice content.
      deepStrictEqual(Buffer.from(data), Buffer.from('hello'));
      serverGot.resolve();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  // Buffer.from('hello') creates a pooled buffer — its backing
  // ArrayBuffer is larger and the view has a non-zero offset.
  const pooledBuf = Buffer.from('hello');
  const id = await clientSession.sendDatagram(pooledBuf);
  notStrictEqual(id, 0n);

  await Promise.all([serverGot.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// --- DGIMP-06: DataView ---
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverGot.promise;
    await serverSession.close();
  }), {
    transportParams: { maxDatagramFrameSize: 1200 },
    ondatagram: mustCall((data) => {
      deepStrictEqual(Buffer.from(data), Buffer.from([0xCA, 0xFE]));
      serverGot.resolve();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxDatagramFrameSize: 1200 },
  });
  await clientSession.opened;

  const ab = new ArrayBuffer(4);
  const fullView = new Uint8Array(ab);
  fullView.set([0xDE, 0xAD, 0xCA, 0xFE]);

  // DataView over bytes [2, 3] of the buffer.
  const dv = new DataView(ab, 2, 2);
  const id = await clientSession.sendDatagram(dv);
  notStrictEqual(id, 0n);

  await Promise.all([serverGot.promise, clientSession.closed]);
  await serverEndpoint.close();
}
