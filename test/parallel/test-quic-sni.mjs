// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

// Use two different keys/certs for the default and SNI host.
const defaultKey = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const defaultCert = fixtures.readKey('agent1-cert.pem');
const sniKey = createPrivateKey(fixtures.readKey('agent2-key.pem'));
const sniCert = fixtures.readKey('agent2-cert.pem');

// Server with SNI: default ('*') uses agent1, 'localhost' uses agent2.
const serverOpened = Promise.withResolvers();
const clientOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then((info) => {
    // The server should see the client's requested servername.
    assert.strictEqual(info.servername, 'localhost');
    serverOpened.resolve();
    serverSession.close();
  }).then(mustCall());
}), {
  sni: {
    '*': { keys: [defaultKey], certs: [defaultCert] },
    'localhost': { keys: [sniKey], certs: [sniCert] },
  },
  alpn: ['quic-test'],
});

assert.ok(serverEndpoint.address !== undefined);

// Client connects with servername 'localhost' — should match the SNI entry.
const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  alpn: 'quic-test',
});
clientSession.opened.then((info) => {
  assert.strictEqual(info.servername, 'localhost');
  clientOpened.resolve();
}).then(mustCall());

await Promise.all([serverOpened.promise, clientOpened.promise]);
clientSession.close();
