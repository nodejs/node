// Flags: --experimental-quic --no-warnings

// stream.destroy(error, options) accepts an explicit `options.code`
// that overrides the default wire code derived from `error`. The
// caller-supplied code is sent on RESET_STREAM (and STOP_SENDING for
// the readable side) so the peer observes exactly that code.
//
// For the test fixture's non-h3 ALPN (`quic-test`), the
// DefaultApplication's `internalErrorCode` is `0x1n`. Without
// `options.code`, a plain `Error` would result in the peer seeing
// `0x1n`. With `options.code = 0x42n`, the peer must see `0x42n`.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverResetSeen = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.onreset = mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
      assert.strictEqual(err.errorCode, 66n);
      serverResetSeen.resolve();
    });

    // The peer's reset causes stream.closed to reject with the reset
    // error code.
    await assert.rejects(stream.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: 'data',
});

const err = new Error('explicit code via options');
const clientClosedAssertion = assert.rejects(stream.closed, err);

// `options.code` (0x42n) takes precedence over the default that
// would have been derived from `error` (which would be the session's
// internalErrorCode, 0x1n for non-h3).
stream.destroy(err, { code: 66n });

await Promise.all([clientClosedAssertion, serverResetSeen.promise]);

await clientSession.close();
await serverEndpoint.close();
