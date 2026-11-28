// Flags: --experimental-dtls --no-warnings

// Test: Multiple clients connecting to the same DTLS server.

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

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

const NUM_CLIENTS = 3;

for (const versions of [V12, V13]) {
  let sessionsAccepted = 0;
  const allClientsConnected = Promise.withResolvers();

  const endpoint = listen(mustCall((session) => {
    session.onmessage = (data) => {
      // Echo back with session identifier.
      session.send(`echo:${data.toString()}`);
    };

    if (++sessionsAccepted === NUM_CLIENTS) {
      allClientsConnected.resolve();
    }
  }, NUM_CLIENTS), {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
    ...versions,
  });

  const serverAddress = endpoint.address;
  const clients = [];
  const clientResponses = [];

  for (let i = 0; i < NUM_CLIENTS; i++) {
    const received = Promise.withResolvers();
    clientResponses.push(received);

    const session = connect('127.0.0.1', serverAddress.port, {
      ca: [ca.toString()],
      rejectUnauthorized: false,
      ...versions,
    });

    session.onmessage = mustCall((data) => {
      assert.strictEqual(data.toString(), `echo:client${i}`);
      received.resolve();
    });

    clients.push(session);
  }

  // Wait for all handshakes.
  await Promise.all(clients.map((c) => c.opened));

  // Send data from each client.
  for (let i = 0; i < NUM_CLIENTS; i++) {
    clients[i].send(`client${i}`);
  }

  // Wait for all echoes.
  await Promise.all(clientResponses.map((r) => r.promise));

  // Clean up.
  await Promise.all(clients.map((c) => c.close()));

  await endpoint.close();
}
