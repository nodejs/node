// Flags: --experimental-quic --no-warnings

// Test: truncation is still reported when the stream is torn down locally
// before the iterator finishes draining. The stream's final read state and
// close error are persisted at close, so a consumer slower than the teardown
// (or one that only starts reading after it) still observes the truncation
// rather than a clean end that would make an incomplete stream look complete.

import { hasQuic, skip } from '../common/index.mjs';
import { setTimeout } from 'node:timers/promises';
import assert from 'node:assert';

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
  assert.ok(received > 0);
  assert.strictEqual(threw, boom);
}

// The session is destroyed without an error mid-iteration: an errorless
// truncation, reported as aborted under the default policy.
{
  const { received, threw } = await readStream(serve, {
    onFirstChunk: ({ session }) => session.destroy(),
  });
  assert.ok(received > 0);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  assert.strictEqual(threw.errorCode, 0n);
}

// Destroying the session before iteration preserves the buffered data,
// but still reports truncation after it has been read.
{
  const { received, threw } = await readStream(serve, {
    beforeIterate: async ({ stream, session }) => {
      while (stream.stats.bytesReceived < 1000n) await setTimeout(5);
      session.destroy();
    },
  });
  assert.strictEqual(received, 1000);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
}

// Destroying the stream itself (rather than the session) before iterating
// reports the truncation the same way, with closed resolving cleanly.
{
  const { received, threw, closedError } = await readStream(serve, {
    beforeIterate: async ({ stream }) => {
      while (stream.stats.bytesReceived < 1000n) await setTimeout(5);
      stream.destroy();
    },
  });
  assert.strictEqual(received, 1000);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  assert.strictEqual(closedError, undefined);
}

// An explicit error must survive draining the buffered data, even when
// truncation itself is ignored. Use more than one reader batch of data.
for (const target of ['session', 'stream']) {
  for (const truncatedReads of ['error', 'ignore']) {
    const size = 128 * 1024;
    const boom = new Error(`${target}: ${truncatedReads}`);
    const { received, threw, closedError } = await readStream((stream) => {
      stream.setBody(stallingBody(size));
    }, {
      clientOptions: {
        truncatedReads,
        transportParams: {
          initialMaxStreamDataBidiLocal: size * 2,
          initialMaxData: size * 2,
        },
      },
      beforeIterate: async (context) => {
        while (context.stream.stats.bytesReceived < BigInt(size)) {
          await setTimeout(5);
        }
        context[target].destroy(boom);
      },
    });
    assert.strictEqual(received, size);
    assert.strictEqual(threw, boom);
    assert.strictEqual(closedError, boom);
  }
}
