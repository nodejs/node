// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// Server offers multiple ALPNs. Client requests one that the server supports.
// Verify the negotiated protocol matches on both sides.

const serverOpened = Promise.withResolvers();
const clientOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then(mustCall((info) => {
    // The server should negotiate proto-b (client's choice from server's list)
    assert.strictEqual(info.protocol, 'proto-b');
    serverOpened.resolve();
    serverSession.close();
  }));
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['proto-a', 'proto-b', 'proto-c'],
});

assert.ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'proto-b',
  servername: 'localhost',
});
clientSession.opened.then(mustCall((info) => {
  assert.strictEqual(info.protocol, 'proto-b');
  clientOpened.resolve();
}));

await Promise.all([serverOpened.promise, clientOpened.promise]);
clientSession.close();
