// Flags: --experimental-quic --experimental-stream-iter --no-warnings --expose-internals

// Test: http3Session.settings
// Verifies that the effective HTTP/3 settings are exposed on the
// Http3Session wrapper, are a null-prototype object, reflect the values
// configured through the `settings` option of http3.connect()/listen()
// (some values are subsequently confirmed/updated by the peer's SETTINGS
// frame; both sides are configured identically here so the values are
// stable), and return null after close. Also pins that h3 sessions
// (unlike raw QUIC sessions) report an installed application.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createRequire } = await import('node:module');
const require = createRequire(import.meta.url);
const { getQuicSessionState } = require('internal/quic/quic');

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const customSettings = {
  maxHeaderPairs: 50n,
  maxHeaderLength: 8192n,
  maxFieldSectionSize: 16384n,
  qpackMaxDTableCapacity: 2048n,
  qpackEncoderMaxDTableCapacity: 2048n,
  qpackBlockedStreams: 50n,
  enableConnectProtocol: false,
};

function checkSettings(settings, what) {
  ok(settings != null, `${what} settings should be available`);
  strictEqual(typeof settings, 'object');
  strictEqual(Object.getPrototypeOf(settings), null);
  strictEqual(settings.maxHeaderPairs, customSettings.maxHeaderPairs);
  strictEqual(settings.maxHeaderLength, customSettings.maxHeaderLength);
  strictEqual(settings.maxFieldSectionSize,
              customSettings.maxFieldSectionSize);
  strictEqual(settings.qpackMaxDtableCapacity,
              customSettings.qpackMaxDTableCapacity);
  strictEqual(settings.qpackEncoderMaxDtableCapacity,
              customSettings.qpackEncoderMaxDTableCapacity);
  strictEqual(settings.qpackBlockedStreams,
              customSettings.qpackBlockedStreams);
  strictEqual(settings.enableConnectProtocol,
              customSettings.enableConnectProtocol);
}

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // The application is installed from the first flight, so the
    // settings are available as soon as the session is surfaced.
    checkSettings(serverSession.settings, 'server');
    strictEqual(
      getQuicSessionState(serverSession.session).hasApplication, true);
    strictEqual(getQuicSessionState(serverSession.session).isServer, true);

    stream.onheaders = mustCall(() => {
      stream.sendHeaders({ ':status': '200' }, { terminal: true });
    });
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  settings: customSettings,
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  settings: customSettings,
});
await clientSession.opened;

// The client installs its application at session creation, so the
// settings are available immediately after the session opens.
checkSettings(clientSession.settings, 'client');
strictEqual(getQuicSessionState(clientSession.session).hasApplication, true);
strictEqual(getQuicSessionState(clientSession.session).isServer, false);

// Exchange a request to let the server side run its assertions.
const stream = await clientSession.request({
  ':method': 'GET',
  ':path': '/',
  ':scheme': 'https',
  ':authority': 'localhost',
});

// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain */ }
await Promise.all([stream.closed, serverDone.promise]);

// After close, settings should return null.
await clientSession.close();
strictEqual(clientSession.settings, null);

await serverEndpoint.close();
