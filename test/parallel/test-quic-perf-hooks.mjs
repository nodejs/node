// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: PerformanceObserver integration for QUIC.
// QuicEndpoint, QuicSession, and QuicStream emit PerformanceEntry
// objects with entryType 'quic' when a PerformanceObserver is active.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';
import { PerformanceObserver } from 'node:perf_hooks';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const entries = [];

const observerDone = Promise.withResolvers();

// Collect all quic perf entries.
const obs = new PerformanceObserver(mustCallAtLeast((list) => {
  for (const entry of list.getEntries()) {
    entries.push(entry);
  }
  // We expect at least: 1 endpoint + 2 sessions + 2 streams = 5 entries.
  // The observer may be called multiple times as entries arrive in batches.
  // Resolve once we have enough entries.
  if (entries.length >= 5) {
    observerDone.resolve();
  }
}));
obs.observe({ entryTypes: ['quic'] });

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('perf test'),
});

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
await serverEndpoint.close();

// Wait for the observer to collect all entries.
await observerDone.promise;
obs.disconnect();

// Verify we got all expected entry types.
const endpointEntries = entries.filter((e) => e.name === 'QuicEndpoint');
const sessionEntries = entries.filter((e) => e.name === 'QuicSession');
const streamEntries = entries.filter((e) => e.name === 'QuicStream');

ok(endpointEntries.length >= 1, `Expected QuicEndpoint entries, got ${endpointEntries.length}`);
ok(sessionEntries.length >= 2, `Expected >= 2 QuicSession entries, got ${sessionEntries.length}`);
ok(streamEntries.length >= 2, `Expected >= 2 QuicStream entries, got ${streamEntries.length}`);

// Verify common fields on all entries.
for (const entry of entries) {
  strictEqual(entry.entryType, 'quic');
  strictEqual(typeof entry.startTime, 'number');
  ok(entry.duration >= 0, `duration should be >= 0, got ${entry.duration}`);
  ok(entry.detail, 'entry should have detail');
  ok(entry.detail.stats, 'entry.detail should have stats');
}

// Verify session-specific detail fields.
for (const entry of sessionEntries) {
  // The handshake may be undefined if destroyed before handshake completes,
  // but in this test both sessions complete handshakes.
  ok(entry.detail.handshake, 'session entry should have handshake info');
  strictEqual(typeof entry.detail.handshake.protocol, 'string');
  strictEqual(typeof entry.detail.handshake.earlyDataAttempted, 'boolean');
  strictEqual(typeof entry.detail.handshake.earlyDataAccepted, 'boolean');
}

// Verify stream-specific detail fields.
for (const entry of streamEntries) {
  ok(entry.detail.direction === 'bidi' || entry.detail.direction === 'uni',
     `stream direction should be bidi or uni, got ${entry.detail.direction}`);
}
