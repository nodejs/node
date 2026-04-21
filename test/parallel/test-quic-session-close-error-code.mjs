// Flags: --experimental-quic --no-warnings

// Test: session close/destroy with application and transport error codes
// .
// Application error propagated as ERR_QUIC_APPLICATION_ERROR.
// Session close with specific app error code — peer receives it.
// Verifies that close() and destroy() with { code, type, reason } options
// send the correct CONNECTION_CLOSE frame, and the peer receives the
// correct error type, code, and reason.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';

const { strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// --- Test 1: close() with application error code ---
// The client closes with an application error. The server receives
// ERR_QUIC_APPLICATION_ERROR with the exact code and reason.
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onerror = mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
      strictEqual(err.message.includes('42n'), true,
                  'error message should contain the code');
      strictEqual(err.message.includes('client shutdown'), true,
                  'error message should contain the reason');
    });
    await rejects(serverSession.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
    serverGot.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await clientSession.opened;

  // Small delay to ensure handshake is fully confirmed so ngtcp2
  // generates the 1-RTT APPLICATION CONNECTION_CLOSE frame.
  await setTimeout(100);

  // close() with application error — the client's closed promise
  // resolves because the close was locally initiated (intentional).
  // The peer receives the error code, but the local side is not in error.
  await clientSession.close({
    code: 42n,
    type: 'application',
    reason: 'client shutdown',
  });

  await serverGot.promise;
  await serverEndpoint.close();
}

// --- Test 2: close() with transport error code ---
// The client closes with a transport error. The server receives
// ERR_QUIC_TRANSPORT_ERROR with the exact code.
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onerror = mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
      strictEqual(err.message.includes('1n'), true,
                  'error message should contain the code');
    });
    await rejects(serverSession.closed, {
      code: 'ERR_QUIC_TRANSPORT_ERROR',
    });
    serverGot.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await clientSession.opened;
  await setTimeout(100);

  // close() with transport error — resolves locally (intentional).
  await clientSession.close({ code: 1n });

  await serverGot.promise;
  await serverEndpoint.close();
}

// --- Test 3: destroy() with application error code ---
// The client destroys with both a JS error and a QUIC error code.
// The peer receives the QUIC application error.
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onerror = mustCall((err) => {
      strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
      strictEqual(err.message.includes('99n'), true);
    });
    await rejects(serverSession.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
    serverGot.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    onerror: mustCall((err) => {
      // The JS error passed to destroy is delivered via onerror.
      strictEqual(err.message, 'fatal error');
    }),
  });
  await clientSession.opened;
  await setTimeout(100);

  const jsError = new Error('fatal error');
  clientSession.destroy(jsError, {
    code: 99n,
    type: 'application',
    reason: 'destroy with code',
  });

  // The closed promise rejects with the JS error, not the QUIC error.
  await rejects(clientSession.closed, jsError);

  await serverGot.promise;
  await serverEndpoint.close();
}

// --- Test 4: close() with no options (default behavior) ---
// Verify the default close sends NO_ERROR and the peer closes cleanly.
{
  const serverGot = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    // No onerror — clean close should not trigger errors.
    await serverSession.closed;
    serverGot.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await clientSession.opened;
  await setTimeout(100);

  // Default close — no error code, clean shutdown.
  await clientSession.close();

  await serverGot.promise;
  await serverEndpoint.close();
}
