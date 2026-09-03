// Flags: --experimental-quic --no-warnings

// Test: a stream read that ends without a FIN is a truncation. What the read
// reports is decided in this order:
//
//   1. the peer reset the stream with a non-zero code: ERR_QUIC_STREAM_RESET
//      carrying that code, under either policy;
//   2. the stream was already being torn down and its closed promise rejected:
//      that same error, under either policy;
//   3. otherwise the truncation carries no error of its own and the
//      truncatedReads policy decides - 'error' (the default) throws
//      ERR_QUIC_STREAM_ABORTED so an incomplete stream can never look
//      complete, 'allow' ends the read cleanly.
//
// This file covers rules 1 and 3 on a live connection: peer resets and local
// aborts. Rule 2 needs the stream to already be tearing down when the reader
// resumes, which is covered by the sibling
// test-quic-stream-truncated-reads-destroy test, and truncation by idle
// timeout by test-quic-stream-truncated-reads-timeout.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { connect, listen, readStream, stallingBody, writeAndAwaitAck } =
  await import('../common/quic.mjs');

// Sends 1000 bytes, waits for them to land, then resets with the given code.
const resetWith = (code) => async (stream) => {
  await writeAndAwaitAck(stream, 1000);
  stream.resetStream(code);
};

// Sends 1000 bytes and then stalls without a FIN, so only the client's own
// abort can end the read.
const stall = (stream) => { stream.setBody(stallingBody(1000)); };

// A peer reset with code 0 is a clean abort: a truncation, but not an error.
// Rule 3 - the default reports it, 'allow' treats it as a clean end.
{
  const { received, threw } = await readStream(resetWith(0n));
  assert.strictEqual(received, 1000);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  assert.strictEqual(threw.errorCode, 0n);
}
{
  const { received, threw } =
    await readStream(resetWith(0n), { clientOptions: { truncatedReads: 'allow' } });
  assert.strictEqual(received, 1000);
  assert.strictEqual(threw, undefined);
}

// A peer reset with a nonzero code is rule 1: an error under either policy,
// carrying the peer's code. It also rejects the closed promise on both sides.
{
  const serverClosed = Promise.withResolvers();
  const { received, threw, closedError } = await readStream(async (stream) => {
    await writeAndAwaitAck(stream, 1000);
    stream.resetStream(42n);
    // Our own reset closes the server-side stream with an error too.
    serverClosed.resolve(stream.closed.then(() => undefined, (err) => err));
  });
  assert.strictEqual(received, 1000);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_RESET');
  assert.strictEqual(threw.errorCode, 42n);
  assert.strictEqual(threw.message,
                     'The QUIC stream was reset by the peer with error code 42');
  assert.strictEqual(closedError?.code, 'ERR_QUIC_APPLICATION_ERROR');
  assert.strictEqual(closedError.errorCode, 42n);
  const serverClosedError = await serverClosed.promise;
  assert.strictEqual(serverClosedError?.code, 'ERR_QUIC_APPLICATION_ERROR');
  assert.strictEqual(serverClosedError.errorCode, 42n);
}
{
  const { received, threw } =
    await readStream(resetWith(42n), { clientOptions: { truncatedReads: 'allow' } });
  assert.strictEqual(received, 1000);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_RESET');
  assert.strictEqual(threw.errorCode, 42n);
}

// Aborting our own read with stopSending() is rule 3, not rule 1: we asked for
// the truncation, so it is not an error the peer inflicted on us, and the code
// we send does not come back as one. The read still stops short, so the
// default policy reports it and 'allow' does not.
{
  const { received, threw, closedError } = await readStream(stall, {
    onFirstChunk: ({ stream }) => stream.stopSending(0n),
  });
  assert.ok(received > 0);
  assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  assert.strictEqual(threw.errorCode, 0n);
  assert.strictEqual(closedError, undefined);
}
{
  const { received, threw } = await readStream(stall, {
    clientOptions: { truncatedReads: 'allow' },
    onFirstChunk: ({ stream }) => stream.stopSending(0n),
  });
  assert.ok(received > 0);
  assert.strictEqual(threw, undefined);
}

// A nonzero stopSending code is an error this end raised, so it is reported
// under either policy and carries that code. The peer answers our
// STOP_SENDING with a RESET_STREAM echoing it, which is what rejects closed -
// but the read reports our own abort rather than attributing it to the peer,
// and does so whether or not that answer has arrived yet.
for (const truncatedReads of ['error', 'allow']) {
  for (const awaitEcho of [false, true]) {
    const peerReset = Promise.withResolvers();
    const { received, threw, closedError } = await readStream(stall, {
      clientOptions: { truncatedReads },
      beforeIterate: ({ stream }) => { stream.onreset = () => peerReset.resolve(); },
      onFirstChunk: async ({ stream }) => {
        stream.stopSending(7n);
        // For the 2nd pass, wait until we receive the corresponding reset:
        if (awaitEcho) await peerReset.promise;
      },
    });
    assert.ok(received > 0);
    assert.strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
    assert.strictEqual(threw.errorCode, 7n);
    assert.strictEqual(closedError?.code, 'ERR_QUIC_APPLICATION_ERROR');
    assert.strictEqual(closedError.errorCode, 7n);
  }
}

// A peer abruptly destroying its session truncates the read too. That
// currently reaches us as an implicit reset, so which rule applies depends on
// the code the peer's teardown puts on the wire - not part of this contract,
// and deliberately not pinned here. What must hold either way is the default
// policy's promise: the incomplete stream never looks complete.
{
  const { received, threw } = await readStream(async (stream, session) => {
    await writeAndAwaitAck(stream, 1000);
    session.destroy(new Error('connection boom'));
  });
  assert.strictEqual(received, 1000);
  assert.ok(threw);
}

// Check the option in the server case as well:
{
  const serverRead = Promise.withResolvers();
  const serverEndpoint = await listen((session) => {
    session.closed.catch(() => {});
    session.onstream = async (stream) => {
      stream.closed.catch(() => {});
      let received = 0;
      try {
        for await (const chunk of stream) {
          for (const c of chunk) received += c.byteLength;
        }
        serverRead.resolve({ received, threw: undefined });
      } catch (err) {
        serverRead.resolve({ received, threw: err });
      }
    };
  }, { truncatedReads: 'allow' });

  const session = await connect(serverEndpoint.address);
  await session.opened;
  session.closed.catch(() => {});

  const stream = await session.createBidirectionalStream();
  stream.closed.catch(() => {});
  await writeAndAwaitAck(stream, 100);
  stream.resetStream(0n);

  const { threw } = await serverRead.promise;
  assert.strictEqual(threw, undefined);

  session.close();
  await serverEndpoint.close();
}

// The option is validated.
await assert.rejects(connect('127.0.0.1:1234', { truncatedReads: 'nope' }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
