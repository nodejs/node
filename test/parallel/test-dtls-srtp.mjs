// Flags: --experimental-dtls --no-warnings

// Test: SRTP profile negotiation and keying-material export (DTLS-SRTP). Both
// peers must negotiate the same SRTP profile and derive identical keying
// material from exportKeyingMaterial.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, deepStrictEqual, notDeepStrictEqual } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const ca = readKey('ca1-cert.pem').toString();

const SRTP_PROFILE = 'SRTP_AEAD_AES_128_GCM';

const gotServerSession = Promise.withResolvers();

const server = listen(mustCall((session) => {
  gotServerSession.resolve(session);
}), {
  cert,
  key,
  port: 0,
  host: '127.0.0.1',
  srtp: SRTP_PROFILE,
});

const client = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: false,
  srtp: SRTP_PROFILE,
});

await client.opened;
const serverSession = await gotServerSession.promise;
await serverSession.opened;

// Both peers negotiate the same SRTP profile.
strictEqual(client.srtpProfile, SRTP_PROFILE);
strictEqual(serverSession.srtpProfile, SRTP_PROFILE);

// Both peers derive identical keying material for the same label and length.
const label = 'EXTRACTOR-dtls_srtp';
const clientKM = client.exportKeyingMaterial(60, label);
const serverKM = serverSession.exportKeyingMaterial(60, label);
strictEqual(clientKM.length, 60);
deepStrictEqual(clientKM, serverKM);

// The optional context is bound into the derivation: the same context yields
// the same material on both peers, and a different context yields different
// material.
const contextA = Buffer.from('context-a');
const contextB = Buffer.from('context-b');
deepStrictEqual(
  client.exportKeyingMaterial(32, label, contextA),
  serverSession.exportKeyingMaterial(32, label, contextA),
);
notDeepStrictEqual(
  client.exportKeyingMaterial(32, label, contextA),
  client.exportKeyingMaterial(32, label, contextB),
);

await client.close();
await server.close();
