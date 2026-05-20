// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { rejects, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// enableEarlyData must be a boolean
await rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  enableEarlyData: 'yes',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// With enableEarlyData: false, early data should not be attempted.
// (Without a session ticket, early data is never attempted regardless,
// but this verifies the option is functional and passes through to C++.)

const serverOpened = Promise.withResolvers();
const clientOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then(mustCall((info) => {
    serverOpened.resolve();
    serverSession.close();
  }));
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  enableEarlyData: false,
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
  enableEarlyData: false,
});
clientSession.opened.then(mustCall((info) => {
  strictEqual(info.earlyDataAttempted, false);
  strictEqual(info.earlyDataAccepted, false);
  clientOpened.resolve();
}));

await Promise.all([serverOpened.promise, clientOpened.promise]);
clientSession.close();
