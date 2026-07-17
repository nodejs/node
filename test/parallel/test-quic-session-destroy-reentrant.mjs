// Flags: --experimental-quic --no-warnings

// Test: QuicSession.destroy and QuicStream.destroy are safely re-entrant.
//
// Calling destroy() from within an onerror callback (or from a stream
// onerror that fires during the session destroy cascade) used to run
// cleanup twice because the `if (this.destroyed)` guard checked `#handle`,
// which was only cleared at the END of destroy() — well after user-visible
// callbacks ran.
//
// The fix introduces a `#destroying` boolean flag that flips
// synchronously at the top of `destroy()`, so a re-entrant call hits
// the guard and returns immediately. The `#handle === undefined`
// invariant remains the "fully torn down" signal used by `kFinishClose`
// and only flips once teardown completes - so `session.destroyed` /
// `stream.destroyed` is still false from inside `onerror` (which runs
// during teardown), and only becomes true once `destroy()` returns.
//
// Cases covered:
//   1. session.onerror calls session.destroy() recursively. Verify the
//      session is not torn down twice (no double publish on the
//      diagnostics_channel, closed promise settles exactly once).
//   2. session.destroy(err) cascades to stream.destroy; the stream's
//      onerror calls session.destroy() again. Verify safe.
//   3. stream.onerror calls stream.destroy() recursively.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import diagnostics_channel from 'node:diagnostics_channel';
import { setImmediate } from 'node:timers/promises';

const { strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// Short idle timeout so the server cleans up quickly after we destroy
// the client without sending CONNECTION_CLOSE.
const transportParams = { maxIdleTimeout: 1 };

// -------------------------------------------------------------------
// 1. session.onerror calls session.destroy() recursively.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  // Count diagnostics_channel publishes. Even though `destroy()` is
  // called twice (once explicitly and once recursively from inside the
  // `onerror` handler), the `#destroying` guard makes the second call
  // a true no-op so each channel publishes exactly once.
  const errSub = mustCall((msg) => {
    strictEqual(msg.session, clientSession);
  });
  const closedSub = mustCall((msg) => {
    strictEqual(msg.session, clientSession);
  });
  diagnostics_channel.subscribe('quic.session.error', errSub);
  diagnostics_channel.subscribe('quic.session.closed', closedSub);

  const testError = new Error('reentrant destroy test');

  clientSession.onerror = mustCall((err) => {
    strictEqual(err, testError);
    // Re-enter destroy synchronously from the error handler. This must
    // be a no-op because `#destroying` is already set; `destroyed` is
    // still false here because `#handle` is cleared at the end of
    // `destroy()`. The fact that the closed promise still rejects with
    // `testError` (asserted below) - and not `'should be ignored'` -
    // proves the recursive call did not run a second teardown.
    clientSession.destroy(new Error('should be ignored'));
  });

  clientSession.destroy(testError);
  // Once `destroy()` has returned, `destroyed` flips to true.
  strictEqual(clientSession.destroyed, true);

  await rejects(clientSession.closed, testError);

  // Give the diagnostics_channel a tick to deliver any deferred messages.
  await setImmediate();

  diagnostics_channel.unsubscribe('quic.session.error', errSub);
  diagnostics_channel.unsubscribe('quic.session.closed', closedSub);

  await serverDone.promise;
  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// 2. session.destroy(err) -> stream onerror -> session.destroy() again.
//    The stream's onerror is invoked during the session's destroy
//    cascade; if that handler calls session.destroy() again, the second
//    destroy must be a no-op.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    // Server-side onstream callbacks are not always invoked here because
    // the client may destroy before the server processes the stream open.
    // We don't make assertions about server stream activity in this case;
    // the test is purely about client-side re-entrancy.
    await serverSession.closed;
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();

  const testError = new Error('cascade reentrant destroy test');

  stream.onerror = mustCall((err) => {
    strictEqual(err, testError);
    // Re-enter session.destroy from inside the stream's onerror. This
    // is happening DURING the session's stream cascade, so the session
    // is mid-destroy. The second destroy() call must be a no-op.
    clientSession.destroy(new Error('should be ignored'));
  });

  clientSession.onerror = mustCall((err) => {
    strictEqual(err, testError);
  });

  clientSession.destroy(testError);
  strictEqual(clientSession.destroyed, true);
  strictEqual(stream.destroyed, true);

  await rejects(clientSession.closed, testError);
  await rejects(stream.closed, testError);

  await serverDone.promise;
  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// 3. stream.onerror calls stream.destroy() recursively.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    try { await serverSession.closed; } catch { /* server cascade-close */ }
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();

  const testError = new Error('stream reentrant destroy test');

  stream.onerror = mustCall((err) => {
    strictEqual(err, testError);
    // Re-enter stream.destroy from inside its own onerror. The
    // `#destroying` flag traps the recursive call; `destroyed` (i.e.
    // `#handle === undefined`) is still false here because the handle
    // is cleared inside `[kFinishClose]` later in this method.
    stream.destroy(new Error('should be ignored'));
  });

  stream.destroy(testError);
  // Once `destroy()` has returned, `destroyed` flips to true.
  strictEqual(stream.destroyed, true);

  await rejects(stream.closed, testError);

  clientSession.close();
  await clientSession.closed;
  await serverDone.promise;
  await serverEndpoint.close();
}
