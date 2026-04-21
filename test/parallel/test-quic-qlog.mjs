// Flags: --experimental-quic --no-warnings

// Test: qlog callback.
// When qlog: true, qlog data is delivered to the session.onqlog
// callback during the connection lifecycle. The final chunk is
// emitted synchronously during ngtcp2_conn destruction with
// fin=true.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setImmediate } from 'node:timers/promises';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const clientChunks = [];
const serverChunks = [];
let clientFinReceived = false;
let serverFinReceived = false;

function assertQlogOutput(chunks, finReceived, side) {
  ok(chunks.length > 0, `Expected ${side} qlog chunks, got ${chunks.length}`);
  ok(finReceived, `Expected ${side} to receive fin`);

  for (const { data, fin } of chunks) {
    strictEqual(typeof data, 'string',
                `Each ${side} qlog chunk should be a string`);
    strictEqual(typeof fin, 'boolean',
                `Each ${side} fin flag should be a boolean`);
  }

  // Only the last chunk should have fin=true.
  for (let i = 0; i < chunks.length - 1; i++) {
    strictEqual(chunks[i].fin, false,
                `${side} chunk ${i} should not be fin`);
  }
  strictEqual(chunks[chunks.length - 1].fin, true,
              `${side} last chunk should be fin`);

  // ngtcp2 emits qlog in JSON-SEQ format (RFC 7464): each record is
  // prefixed with 0x1e (Record Separator) and terminated by a newline.
  // Parse the individual records and verify the header has expected fields.
  const joined = chunks.map((c) => c.data).join('');
  const records = joined.split('\x1e').filter((s) => s.trim().length > 0);
  ok(records.length > 0, `${side} qlog should have at least one record`);

  // The first record is the qlog header with format metadata.
  const header = JSON.parse(records[0]);

  ok(header.qlog_version !== undefined || header.qlog_format !== undefined,
     `${side} qlog header should have qlog_version or qlog_format field`);

  for (let i = 1; i < records.length; i++) {
    const record = JSON.parse(records[i]);
    ok('name' in record);
    ok('data' in record);
    ok('time' in record);
  }
}

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  serverSession.close();
}), {
  qlog: true,
  onqlog(data, fin) {
    serverChunks.push({ data, fin });
    if (fin) serverFinReceived = true;
  },
});

const clientSession = await connect(serverEndpoint.address, {
  qlog: true,
  onqlog(data, fin) {
    clientChunks.push({ data, fin });
    if (fin) clientFinReceived = true;
  },
});

await Promise.all([clientSession.opened, clientSession.closed]);
await serverEndpoint.close();

// The final qlog chunk (fin=true) is delivered via SetImmediate because
// it is emitted during ngtcp2_conn destruction when MakeCallback is
// unsafe. Yield to let the deferred callback run before asserting.
await setImmediate();

assertQlogOutput(clientChunks, clientFinReceived, 'client');
assertQlogOutput(serverChunks, serverFinReceived, 'server');
