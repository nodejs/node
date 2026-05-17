// Flags: --experimental-quic --no-warnings

// Test: CRL (certificate revocation list) enforcement.
// A server using a non-revoked certificate succeeds when the client
// provides a CRL. A server using the same certificate reports
// "certificate revoked" in the validation error when the client
// provides a CRL that revokes it.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const ca2Cert = readKey('ca2-cert.pem');
const ca2Crl = readKey('ca2-crl.pem');
const ca2CrlAgent3 = readKey('ca2-crl-agent3.pem');
const agent3Key = createPrivateKey(readKey('agent3-key.pem'));
const agent3Cert = readKey('agent3-cert.pem');

// --- Non-revoked: agent3 with original CRL (doesn't list agent3) ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    await serverSession.close();
  }), {
    sni: { '*': { keys: [agent3Key], certs: [agent3Cert] } },
    alpn: ['quic-test'],
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    ca: [ca2Cert],
    crl: [ca2Crl],
  });

  // Should succeed — agent3 is NOT in the original CRL.
  const info = await clientSession.opened;
  strictEqual(clientSession.destroyed, false);
  // No revocation error.
  ok(!info.validationErrorReason ||
     !info.validationErrorReason.includes('revoked'));
  await clientSession.close();
  await serverEndpoint.close();
}

// --- Revoked: agent3 with CRL that revokes agent3 ---
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    await serverSession.close();
  }), {
    sni: { '*': { keys: [agent3Key], certs: [agent3Cert] } },
    alpn: ['quic-test'],
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    ca: [ca2Cert],
    crl: [ca2CrlAgent3],
  });

  // The connection currently succeeds but the validation error
  // reports "certificate revoked". This verifies the CRL is loaded
  // and checked.
  const info = await clientSession.opened;
  strictEqual(info.validationErrorReason, 'certificate revoked');
  await clientSession.close();
  await serverEndpoint.close();
}
