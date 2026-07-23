// Flags: --experimental-quic --no-warnings

// Test: truncation is still reported when the stream is torn down locally
// before the iterator finishes draining. The stream's final read state and
// close error are persisted at close, so a consumer slower than the teardown
// (or one that only starts reading after it) still observes the truncation
// rather than a clean end that would make an incomplete stream look complete.

import { hasQuic, skip } from '../common/index.mjs';
import { setTimeout } from 'node:timers/promises';
import assert from 'node:assert';

const { strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { readStream, stallingBody } = await import('../common/quic.mjs');

// The server sends 1000 bytes then stalls without a FIN, so the client's
// read side can only end by truncation.
const serve = (stream) => { stream.setBody(stallingBody(1000)); };

// The session is destroyed with an error while the consumer is mid-iteration:
// the iterator delivers the data that arrived, then throws that same error
// object.
{
  const boom = new Error('boom');
  const { received, threw } = await readStream(serve, {
    onFirstChunk: ({ session }) => session.destroy(boom),
  });
  ok(received > 0);
  strictEqual(threw, boom);
}

// The session is destroyed without an error mid-iteration: an errorless
// truncation, reported as aborted under the default policy.
{
  const { received, threw } = await readStream(serve, {
    onFirstChunk: ({ session }) => session.destroy(),
  });
  ok(received > 0);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  strictEqual(threw.errorCode, 0n);
}

// The session is destroyed before iteration even starts: the buffered data
// is gone, but the truncation is still reported rather than a clean empty
// end.
{
  const { received, threw } = await readStream(serve, {
    beforeIterate: async ({ stream, session }) => {
      while (stream.stats.bytesReceived === 0n) await setTimeout(5);
      session.destroy();
    },
  });
  strictEqual(received, 0);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
}

// Destroying the stream itself (rather than the session) before iterating
// reports the truncation the same way, with closed resolving cleanly.
{
  const { received, threw, closedError } = await readStream(serve, {
    beforeIterate: async ({ stream }) => {
      while (stream.stats.bytesReceived === 0n) await setTimeout(5);
      stream.destroy();
    },
  });
  strictEqual(received, 0);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  strictEqual(closedError, undefined);
}
