// Flags: --experimental-quic --no-warnings

// Test: terminating a worker thread that still holds live QUIC sessions.
//
// worker.terminate() tears the environment down without running any of the
// JavaScript close paths, so the sessions are still open when the QUIC
// binding is cleaned up. Sessions must be properly destroyed before reaching
// ~Session.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { Worker, isMainThread, parentPort } from 'node:worker_threads';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// Launch a client and server in a worker thread, then kill it:
if (!isMainThread) {
  // A client and a server session, both with an open stream, and neither
  // closed. The worker then parks forever waiting to be terminated.
  const serverEndpoint = await listen((session) => {
    session.closed.catch(() => {});
    session.onstream = (stream) => { stream.closed.catch(() => {}); };
  });

  const clientSession = await connect(serverEndpoint.address);
  clientSession.closed.catch(() => {});
  await clientSession.opened;
  const stream = await clientSession.createBidirectionalStream({
    body: new Uint8Array(1),
  });
  stream.closed.catch(() => {});

  parentPort.postMessage('ready');
  await new Promise(() => {});
} else {
  const worker = new Worker(new URL(import.meta.url));
  worker.on('error', (err) => { assert.fail(err); });
  worker.on('message', mustCall(async (message) => {
    assert.strictEqual(message, 'ready');
    assert.strictEqual(await worker.terminate(), 1);
  }));
}
