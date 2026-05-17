// Flags: --experimental-quic --no-warnings

// Test: version negotiation.
// Version mismatch triggers version negotiation.
// Client receives ERR_QUIC_VERSION_NEGOTIATION_ERROR.
// quic.session.version.negotiation diagnostics channel fires.
// The client connects with an unsupported version number. The server
// responds with a Version Negotiation packet. The client's closed
// promise rejects with ERR_QUIC_VERSION_NEGOTIATION_ERROR.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const bogusVersion = 0x1a1a1a1a;

// Subscribe to the version negotiation diagnostics channel.
const channelFired = Promise.withResolvers();
dc.subscribe('quic.session.version.negotiation', mustCall((msg) => {
  ok(msg.session, 'message should have session');
  strictEqual(msg.version, bogusVersion);
  ok(Array.isArray(msg.requestedVersions),
     'requestedVersions should be an array');
  ok(msg.requestedVersions.length > 0,
     'server should advertise at least one version');
  ok(Array.isArray(msg.supportedVersions),
     'supportedVersions should be an array');
  channelFired.resolve();
}));

const serverEndpoint = await listen(async (serverSession) => {
  // The server should never create a session for an unsupported version.
  assert.fail('Server session callback should not be called');
});

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
  // Use an unsupported version to trigger version negotiation.
  version: bogusVersion,
  // The onversionnegotiation callback fires with version info.
  onversionnegotiation: mustCall((version, requestedVersions,
                                  supportedVersions) => {
    // The version is the bogus version we configured.
    strictEqual(version, bogusVersion);
    // requestedVersions are the versions the server advertised in
    // the Version Negotiation packet.
    ok(Array.isArray(requestedVersions),
       'requestedVersions should be an array');
    ok(requestedVersions.length > 0,
       'server should advertise at least one supported version');
    // supportedVersions is our local supported range [min, max].
    ok(Array.isArray(supportedVersions),
       'supportedVersions should be an array');
    strictEqual(supportedVersions.length, 2,
                'supportedVersions should have [min, max]');
  }),
  // The onerror callback fires with the version negotiation error.
  onerror: mustCall((err) => {
    strictEqual(err.code, 'ERR_QUIC_VERSION_NEGOTIATION_ERROR');
  }),
});

// The closed promise rejects with ERR_QUIC_VERSION_NEGOTIATION_ERROR.
await assert.rejects(clientSession.closed, {
  code: 'ERR_QUIC_VERSION_NEGOTIATION_ERROR',
});

// Wait for the diagnostics channel to fire.
await channelFired.promise;

await serverEndpoint.close();
