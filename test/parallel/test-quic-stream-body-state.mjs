// Flags: --experimental-quic --no-warnings

// Test: setBody / writer mutual exclusion.
// setBody() after writer accessed throws.
// writer after setBody() throws.
// setBody() on destroyed stream throws.
// BODY-17 (setBody twice throws) is already covered by
// test-quic-stream-bidi-setbody.mjs.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

let streamCount = 0;
// BODY-20 destroys the stream before data is sent, so the server only sees 2.
const totalStreams = 2;
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Close the server's write side so the stream can fully close.
    stream.writer.endSync();
    await stream.closed;
    if (++streamCount === totalStreams) {
      serverSession.close();
      allDone.resolve();
    }
  }, totalStreams);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// setBody() after writer accessed throws.
{
  const stream = await clientSession.createBidirectionalStream();
  // Access the writer — this initializes the streaming source.
  const w = stream.writer;
  ok(w);
  throws(() => {
    stream.setBody(encoder.encode('too late'));
  }, {
    code: 'ERR_INVALID_STATE',
  });
  w.endSync();
  await stream.closed;
}

// Writer after setBody() throws.
{
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(encoder.encode('body set'));
  throws(() => {
    stream.writer; // eslint-disable-line no-unused-expressions
  }, {
    code: 'ERR_INVALID_STATE',
  });
  await stream.closed;
}

// setBody() on destroyed stream throws.
{
  const stream = await clientSession.createBidirectionalStream();
  stream.destroy();
  throws(() => {
    stream.setBody(encoder.encode('destroyed'));
  }, {
    code: 'ERR_INVALID_STATE',
  });
  // stream.closed resolves (destroy without error).
  await stream.closed;
}

await allDone.promise;
await clientSession.close();
await serverEndpoint.close();
