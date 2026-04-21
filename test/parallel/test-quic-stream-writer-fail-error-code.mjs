// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream writer.fail() emits a RESET_STREAM with a non-zero
// application error code instead of the previously hard-coded 0n.
//
// Two cases are exercised against the test fixture's non-h3 ALPN
// (`quic-test`), which selects the C++ DefaultApplication and exposes
// `internalErrorCode === 0x1n` (NGTCP2_INTERNAL_ERROR) via session
// state:
//
//   1. `writer.fail(plainError)` — peer receives RESET_STREAM with the
//      session's `internalErrorCode` (`0x1`), proving the hard-coded
//      `0n` regression is gone and that the C++ -> JS state plumbing
//      surfaces the application's code.
//   2. `writer.fail(new QuicError('msg', { errorCode: 0x42n }))` —
//      peer receives RESET_STREAM with the explicit code, proving
//      the QuicError fast path.
//
// The peer-side observation goes through `stream.onreset(err)` where
// `err` is `ERR_QUIC_APPLICATION_ERROR` carrying the wire code in its
// message string. We extract the code via regex; once
// `ERR_QUIC_APPLICATION_ERROR` exposes the numeric code as a property
// (a planned follow-up), this test can switch to direct property
// access.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { QuicError } = await import('node:quic');

// Extract the numeric wire code from an ERR_QUIC_APPLICATION_ERROR
// message of the form
//   "A QUIC application error occurred. <code>n [<reason>]"
// where the trailing `n` on the code is the BigInt formatting from
// `util.format('%d', bigint)`. RESET_STREAM frames do not carry a
// reason string, so the bracketed value is typically `undefined`.
function wireCodeOf(err) {
  strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
  const match = err.message.match(/A QUIC application error occurred\. (\d+)n /);
  if (!match) {
    throw new Error(`Could not extract code from message: ${err.message}`);
  }
  return BigInt(match[1]);
}

// Server: capture the next two streams. Each stream receives an
// onreset handler synchronously inside onstream so the C++ -> JS
// dispatch ordering (data packet -> reset packet) finds the handler
// already attached.
const expectedCodes = [0x1n, 0x42n];
let nextStreamIndex = 0;
const allDone = Promise.withResolvers();
const observed = [];

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    const i = nextStreamIndex++;
    stream.onreset = mustCall((err) => {
      observed[i] = wireCodeOf(err);
      if (observed.length === expectedCodes.length &&
          observed[0] !== undefined && observed[1] !== undefined) {
        allDone.resolve();
      }
    });
  }, 2);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// 1. Plain Error -> session.internalErrorCode (0x1n for non-h3).
{
  const stream = await clientSession.createBidirectionalStream();
  const writer = stream.writer;
  // Write a tiny chunk to guarantee the server-side stream is
  // created via onstream before the RESET_STREAM frame arrives.
  writer.writeSync('x');
  writer.fail(new Error('plain error reason'));
}

// 2. QuicError with explicit code -> peer sees that exact code.
{
  const stream = await clientSession.createBidirectionalStream();
  const writer = stream.writer;
  writer.writeSync('y');
  writer.fail(new QuicError('explicit code', { errorCode: 0x42n }));
}

await allDone.promise;

strictEqual(observed[0], expectedCodes[0]);
strictEqual(observed[1], expectedCodes[1]);

await clientSession.close();
await serverEndpoint.close();
