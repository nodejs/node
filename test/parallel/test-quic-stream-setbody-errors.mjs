// Flags: --experimental-quic --no-warnings

// Test: setBody throws when body already configured or writer
// already accessed.
// Writer throws ERR_INVALID_STATE if body was already set.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Test 1: setBody after setBody throws.
{
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(encoder.encode('first'));

  throws(() => stream.setBody(encoder.encode('second')), {
    code: 'ERR_INVALID_STATE',
    message: /outbound already configured/,
  });

  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Test 2: setBody after writer accessed throws.
{
  const stream = await clientSession.createBidirectionalStream();
  // Access the writer — this prevents setBody from being used.
  const w = stream.writer;
  w.endSync();

  throws(() => stream.setBody(encoder.encode('data')), {
    code: 'ERR_INVALID_STATE',
    message: /writer already accessed/,
  });

  // The server handles only the first stream and then closes its session, so
  // this stream is never answered and never receives a FIN. Reading it
  // therefore surfaces the truncation rather than ending cleanly.
  await assert.rejects((async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream) { /* drain */ }
  })(), { code: 'ERR_QUIC_STREAM_ABORTED' });
  await stream.closed;
}

await clientSession.close();
await serverEndpoint.close();
