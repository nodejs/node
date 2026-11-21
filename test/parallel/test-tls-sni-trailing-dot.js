'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A server name with a terminal root dot refers to the same host as the
// name without it, so it must select the same SNI context. The context
// that was selected is detected from the server certificate presented to
// the client.

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const defaultCreds = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};
const sniCreds = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
};

function getServerCommonName(port, servername, version) {
  return new Promise((resolve, reject) => {
    const socket = tls.connect({
      port,
      servername,
      rejectUnauthorized: false,
      minVersion: version,
      maxVersion: version,
    }, common.mustCall(() => {
      const { CN } = socket.getPeerCertificate().subject;
      socket.end();
      resolve(CN);
    }));
    socket.on('error', reject);
  });
}

const cases = [
  // [registered name, names a client may send]
  ['host.example', ['host.example', 'host.example.']],
  ['host.example.', ['host.example', 'host.example.']],
  ['*.host.example', ['a.host.example', 'a.host.example.']],
];

async function test(version, registered, servernames) {
  const server = tls.createServer({
    ...defaultCreds,
    minVersion: version,
    maxVersion: version,
  }, (socket) => socket.end());
  server.addContext(registered, sniCreds);
  await new Promise((resolve) => server.listen(0, resolve));

  const { port } = server.address();
  for (const servername of servernames) {
    assert.strictEqual(await getServerCommonName(port, servername, version),
                       'agent2', `${version} ${registered} ${servername}`);
  }

  // A name that matches no context still uses the default one.
  assert.strictEqual(await getServerCommonName(port, 'other.example.', version),
                     'agent1', `${version} ${registered} other.example.`);
  await new Promise((resolve) => server.close(resolve));
}

(async () => {
  for (const version of ['TLSv1.2', 'TLSv1.3']) {
    for (const [registered, servernames] of cases)
      await test(version, registered, servernames);
  }
})().then(common.mustCall());
