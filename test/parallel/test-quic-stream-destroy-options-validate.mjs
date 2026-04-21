// Flags: --experimental-quic --no-warnings

// stream.destroy(error, options) validates `options` up front, before
// any side effects. A malformed `options` argument must throw without
// mutating `#destroying`, emitting wire frames, invoking `onerror`,
// or settling the `closed` promise. After the throw, the stream is
// still alive and a subsequent destroy with valid options must
// succeed.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, throws, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(() => true));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();

stream.onerror = mustNotCall(
  'stream.onerror must not fire when destroy() throws on bad options');

// 1. options is not an object -> throws ERR_INVALID_ARG_TYPE.
throws(() => stream.destroy(new Error('x'), 'not an object'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(stream.destroyed, false);

// 2. options.code is the wrong type -> throws ERR_INVALID_ARG_TYPE.
throws(() => stream.destroy(new Error('x'), { code: 'oops' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(stream.destroyed, false);

throws(() => stream.destroy(new Error('x'), { code: true }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(stream.destroyed, false);

// 3. options.reason is the wrong type -> throws ERR_INVALID_ARG_TYPE.
throws(() => stream.destroy(new Error('x'), { reason: 42 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
strictEqual(stream.destroyed, false);

// Switch to the real error handler before the final destroy so the
// `mustNotCall` above does not fire on the legitimate teardown.
const finalError = new Error('final destroy');
stream.onerror = mustCall((err) => { strictEqual(err, finalError); });

const clientClosedAssertion = rejects(stream.closed, finalError);

// 4. Valid options accepted: bigint code.
stream.destroy(finalError, { code: 0x10n, reason: 'cleanup' });
strictEqual(stream.destroyed, true);

// 5. Re-entry with arbitrarily bad options is a no-op (the
//    re-entrancy guard returns before validation runs).
stream.destroy(new Error('after-destroy'), { code: 'still-bad' });
strictEqual(stream.destroyed, true);

await clientClosedAssertion;

await clientSession.close();
await serverEndpoint.close();
