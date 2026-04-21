// Flags: --experimental-quic --no-warnings

// Test: session properties (PATH-03, PATH-06, PATH-07, PATH-08,
// CERT-01, CERT-02, CERT-03, CERT-04, CERT-05).
// PATH-03/06: session.path returns { local, remote } with addresses.
// session.path is cached (same object on second access).
// session.path returns undefined after destroy.
// session.certificate returns own cert object.
// session.peerCertificate returns peer cert.
// session.ephemeralKeyInfo returns key info on client.
// All three cached.
// All three return undefined after destroy.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;

  // PATH-03/06: Server path has local and remote.
  const path = serverSession.path;
  ok(path);
  ok(path.local);
  ok(path.remote);

  // Cached.
  strictEqual(serverSession.path, path);

  // Own certificate.
  const cert = serverSession.certificate;
  ok(cert);

  // Peer certificate (client's cert — not set in this
  // test since we don't use verifyClient, so it's undefined).
  strictEqual(serverSession.peerCertificate, undefined);

  // Cached.
  strictEqual(serverSession.certificate, cert);

  await serverSession.close();
  serverDone.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// PATH-03/06: Client path.
const path = clientSession.path;
ok(path);
ok(path.local);
ok(path.remote);

// Cached.
strictEqual(clientSession.path, path);

// Peer certificate (server's cert).
const peerCert = clientSession.peerCertificate;
ok(peerCert);

// Ephemeral key info (client only).
const keyInfo = clientSession.ephemeralKeyInfo;
ok(keyInfo);

// Cached.
strictEqual(clientSession.peerCertificate, peerCert);
strictEqual(clientSession.ephemeralKeyInfo, keyInfo);

await Promise.all([clientSession.closed, serverDone.promise]);

// Returns undefined after destroy.
strictEqual(clientSession.path, undefined);

// Returns undefined after destroy.
strictEqual(clientSession.certificate, undefined);
strictEqual(clientSession.peerCertificate, undefined);
strictEqual(clientSession.ephemeralKeyInfo, undefined);

await serverEndpoint.close();
