// Flags: --experimental-dtls --no-warnings

// Test that DTLS uses Node's configurable default CA set when no explicit
// `ca` option is provided.

import { hasCrypto, mustCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');
const { setDefaultCACertificates } = await import('node:tls');

const serverCert = fixtures.readKey('agent1-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

setDefaultCACertificates([ca]);

const endpoint = listen(mustCall(), {
  // Also cover the shared certificate-chain loader adopted by DTLS.
  cert: Buffer.concat([serverCert, ca]),
  key: serverKey,
  host: '127.0.0.1',
  port: 0,
});

// `servername` is both the SNI value and the identity checked during
// certificate verification. agent1-cert.pem has no subjectAltName, so the
// identity has to be matched against its CN; connecting to the IP literal
// alone would fail with X509_V_ERR_IP_ADDRESS_MISMATCH before the CA set is
// ever exercised.
const client = connect('127.0.0.1', endpoint.address.port, {
  servername: 'agent1',
});
await client.opened;

await client.close();
await endpoint.close();
