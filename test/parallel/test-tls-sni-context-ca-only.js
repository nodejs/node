'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// An SNI context that sets `ca` but no certificate must still replace the
// default trust store for client certificates on that host. A context that
// sets neither keeps the default trust store.

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const serverCreds = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
};
const ca1 = fixtures.readKey('ca1-cert.pem');
const ca2 = fixtures.readKey('ca2-cert.pem');
const clients = {
  agent1: {  // Issued by CA1.
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  },
  agent3: {  // Issued by CA2.
    key: fixtures.readKey('agent3-key.pem'),
    cert: fixtures.readKey('agent3-cert.pem'),
  },
};

const cases = [
  { name: 'ca only', context: { ca: ca2 }, accepted: 'agent3' },
  { name: 'cert, key and ca', context: { ...serverCreds, ca: ca2 },
    accepted: 'agent3' },
  { name: 'no ca or cert', context: { ciphers: 'DEFAULT' },
    accepted: 'agent1' },
];

function connect(port, client, version) {
  return new Promise((resolve) => {
    const socket = tls.connect({
      port,
      servername: 'protected.example',
      ...clients[client],
      rejectUnauthorized: false,
      minVersion: version,
      maxVersion: version,
    });
    let data = '';
    socket.setEncoding('utf8');
    socket.on('data', (chunk) => data += chunk);
    socket.on('error', () => {});
    socket.on('close', () => resolve(data));
  });
}

async function test(version, { name, context, accepted }) {
  const server = tls.createServer({
    ...serverCreds,
    ca: ca1,
    requestCert: true,
    rejectUnauthorized: true,
    minVersion: version,
    maxVersion: version,
  }, common.mustCall((socket) => {
    assert.strictEqual(socket.servername, 'protected.example');
    socket.end(socket.getPeerCertificate().subject.CN);
  }));
  server.on('tlsClientError', () => {});
  server.addContext('protected.example', context);
  await new Promise((resolve) => server.listen(0, resolve));

  const { port } = server.address();
  for (const client of Object.keys(clients)) {
    const expected = client === accepted ? client : '';
    assert.strictEqual(await connect(port, client, version), expected,
                       `${version}, ${name}: ${client}`);
  }
  server.close();
}

(async () => {
  for (const version of ['TLSv1.2', 'TLSv1.3']) {
    for (const c of cases)
      await test(version, c);
  }
})().then(common.mustCall());
