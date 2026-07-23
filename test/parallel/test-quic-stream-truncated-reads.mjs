// Flags: --experimental-quic --no-warnings

// Test: a stream read that ends without a FIN is a truncation, reported per
// the truncatedReads session option. The default 'error' reports every
// truncation, so an incomplete stream can never look complete. 'allow'
// reports only a truncation that carries an error - a peer reset with a
// nonzero code, or a connection error - and treats an errorless one as a
// clean end. This file covers truncations on a live connection; truncation
// by idle timeout and by local teardown are covered by the sibling
// test-quic-stream-truncated-reads-{timeout,destroy} tests.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { connect, readStream, writeAndAwaitAck } =
  await import('../common/quic.mjs');

// Sends 1000 bytes, waits for them to land, then resets with the given code.
const resetWith = (code) => async (stream) => {
  await writeAndAwaitAck(stream, 1000);
  stream.resetStream(code);
};

// A peer reset with code 0 is a clean abort: a truncation, but not an error.
// The default reports it; 'allow' treats it as a clean end.
{
  const { received, threw } = await readStream(resetWith(0n));
  strictEqual(received, 1000);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  strictEqual(threw.errorCode, 0n);
}
{
  const { received, threw } =
    await readStream(resetWith(0n), { clientOptions: { truncatedReads: 'allow' } });
  strictEqual(received, 1000);
  strictEqual(threw, undefined);
}

// A peer reset with a nonzero code is an error under either policy, carrying
// the peer's code. It also rejects the closed promise on both sides.
{
  const serverClosed = Promise.withResolvers();
  const { received, threw, closedError } = await readStream(async (stream) => {
    await writeAndAwaitAck(stream, 1000);
    stream.resetStream(42n);
    // Our own reset closes the server-side stream with an error too.
    serverClosed.resolve(stream.closed.then(() => undefined, (err) => err));
  });
  strictEqual(received, 1000);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_RESET');
  strictEqual(threw.errorCode, 42n);
  strictEqual(threw.message,
              'The QUIC stream was reset by the peer with error code 42');
  strictEqual(closedError?.code, 'ERR_QUIC_APPLICATION_ERROR');
  strictEqual(closedError.errorCode, 42n);
  const serverClosedError = await serverClosed.promise;
  strictEqual(serverClosedError?.code, 'ERR_QUIC_APPLICATION_ERROR');
  strictEqual(serverClosedError.errorCode, 42n);
}
{
  const { received, threw } =
    await readStream(resetWith(42n), { clientOptions: { truncatedReads: 'allow' } });
  strictEqual(received, 1000);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_RESET');
  strictEqual(threw.errorCode, 42n);
}

// A connection error is reported on the read itself, with its real type,
// under both policies.
for (const truncatedReads of ['error', 'allow']) {
  const { received, threw, closedError } = await readStream(
    async (stream, session) => {
      await writeAndAwaitAck(stream, 1000);
      session.destroy(new Error('connection boom'));
    }, { clientOptions: { truncatedReads } });
  strictEqual(received, 1000);
  strictEqual(threw?.code, 'ERR_QUIC_TRANSPORT_ERROR');
  // The read surfaces the same error that rejects closed.
  strictEqual(threw, closedError);
}

// The option is validated.
await rejects(connect('127.0.0.1:1234', { truncatedReads: 'nope' }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
