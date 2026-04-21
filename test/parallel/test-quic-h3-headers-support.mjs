// Flags: --experimental-quic --no-warnings

// Test: Headers support detection for non-H3 sessions.
// headersSupported is UNSUPPORTED for non-H3 sessions
// Sending headers on non-H3 session throws ERR_INVALID_STATE
// Setting header callbacks on non-H3 stream throws ERR_INVALID_STATE

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Sending headers on non-H3 stream throws.
    throws(() => {
      stream.sendHeaders({ ':status': '200' });
    }, { code: 'ERR_INVALID_STATE' });

    // Setting onheaders on non-H3 stream throws.
    throws(() => {
      stream.onheaders = () => {};
    }, { code: 'ERR_INVALID_STATE' });

    // Setting ontrailers on non-H3 stream throws.
    throws(() => {
      stream.ontrailers = () => {};
    }, { code: 'ERR_INVALID_STATE' });

    // Setting oninfo on non-H3 stream throws.
    throws(() => {
      stream.oninfo = () => {};
    }, { code: 'ERR_INVALID_STATE' });

    // Setting onwanttrailers on non-H3 stream throws.
    throws(() => {
      stream.onwanttrailers = () => {};
    }, { code: 'ERR_INVALID_STATE' });

    // sendInformationalHeaders throws on non-H3.
    throws(() => {
      stream.sendInformationalHeaders({ ':status': '103' });
    }, { code: 'ERR_INVALID_STATE' });

    // sendTrailers throws on non-H3.
    throws(() => {
      stream.sendTrailers({ 'x-trailer': 'value' });
    }, { code: 'ERR_INVALID_STATE' });

    try { await stream.closed; } catch {
      // Stream may close with error.
    }
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: 'quic-test',
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  alpn: 'quic-test',
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('ping'),
});
stream.closed.catch(() => {});

// Client side — sending headers on non-H3 stream throws.
throws(() => {
  stream.sendHeaders({ ':method': 'GET' });
}, { code: 'ERR_INVALID_STATE' });

try { await stream.closed; } catch {
  // Stream may close with error.
}
await serverDone.promise;
clientSession.close();
