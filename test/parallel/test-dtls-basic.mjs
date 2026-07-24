// Flags: --experimental-dtls --no-warnings

// Test: Basic DTLS handshake and bidirectional data exchange.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const serverCert = fixtures.readKey('agent1-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const serverReceivedData = Promise.withResolvers();
const clientReceivedData = Promise.withResolvers();

let serverHandshakeDone = false;
let clientHandshakeDone = false;

// Start server.
const endpoint = listen(mustCall((session) => {
  session.onmessage = mustCall((data) => {
    assert.strictEqual(data.toString(), 'hello from client');
    serverReceivedData.resolve();

    // Send response back to client.
    session.send('hello from server');
  });

  session.onhandshake = mustCall((protocol) => {
    assert.ok(protocol);
    assert.match(protocol, /DTLS/i);
    serverHandshakeDone = true;
  });
}), {
  cert: serverCert.toString(),
  key: serverKey.toString(),
  port: 0,
  host: '127.0.0.1',
});

const serverAddress = endpoint.address;
assert.ok(serverAddress);
assert.ok(serverAddress.port > 0);

// Connect client.
const clientSession = connect('127.0.0.1', serverAddress.port, {
  ca: [ca.toString()],
  rejectUnauthorized: false,
});

clientSession.onmessage = mustCall((data) => {
  assert.strictEqual(data.toString(), 'hello from server');
  clientReceivedData.resolve();
});

clientSession.onhandshake = mustCall((protocol) => {
  assert.ok(protocol);
  clientHandshakeDone = true;
});

// Wait for handshake.
const { protocol } = await clientSession.opened;
assert.match(protocol, /DTLS/i);

// Send data.
clientSession.send('hello from client');

// Wait for bidirectional exchange.
await Promise.all([serverReceivedData.promise, clientReceivedData.promise]);

// Verify handshakes completed.
assert.ok(clientHandshakeDone);
assert.ok(serverHandshakeDone);

// Clean up.
await clientSession.close();
await endpoint.close();
