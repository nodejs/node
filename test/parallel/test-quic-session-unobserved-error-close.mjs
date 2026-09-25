// Flags: --experimental-quic --no-warnings

// Regression test: destroying a session with an error while its `closed`
// promise is not being observed must NOT surface as an unhandled rejection.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import { subscribe, unsubscribe } from 'node:diagnostics_channel';
import { setImmediate as tick } from 'node:timers/promises';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// Any unhandled rejection - e.g. an unobserved `closed` rejecting - fails.
process.on('unhandledRejection', mustNotCall('unexpected unhandled rejection'));

// We use the diagnostics channel to observe the error close without actually
// observing session.closed (not listening is the whole point of the test):
const serverErrored = Promise.withResolvers();
function onSessionError() {
  unsubscribe('quic.session.error', onSessionError);
  serverErrored.resolve();
}
subscribe('quic.session.error', onSessionError);

// The server session callback is left deliberately empty, so no response
// is sent and closed remains unobserved:
const serverEndpoint = await listen(mustCall());

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Finish handshake before close:
await tick();

// Cleanly close from the client with an error code, so the server
// receives a peer error close:
await clientSession.close({ code: 1 });

// Wait until the server has processed the error close, plus another tick
// to ensure unobserved promise rejection doesn't fire anywhere:
await serverErrored.promise;
await tick();

await serverEndpoint.close();
