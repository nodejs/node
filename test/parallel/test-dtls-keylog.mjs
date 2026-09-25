// Flags: --experimental-dtls --no-warnings --expose-internals

// Test: the onkeylog callback delivers NSS-format key material during the
// handshake (useful for decrypting captures in Wireshark), and that key
// material is only extracted when something is listening for it.

import { hasCrypto, mustCall, mustCallAtLeast, mustNotCall, skip } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

// The state and sessions views are not public API; they are reached here the
// way node:quic's tests reach theirs.
const {
  getDTLSSessionState,
} = (await import('internal/dtls/dtls')).default;

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();

const gotKeylog = Promise.withResolvers();

const server = listen(mustCall(), {
  cert, key, port: 0, host: '127.0.0.1',
});

const client = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: false,
});

// state.hasKeylogListener is the flag the native keylog callback reads to
// decide whether to extract key material at all. Without a listener the
// secrets stay inside OpenSSL and are never turned into JS strings, where
// they would be reachable from heap snapshots and core dumps.
assert.strictEqual(getDTLSSessionState(client).hasKeylogListener, false);

// A keylog line is "<LABEL> <hex> <hex>" (e.g. "CLIENT_RANDOM ...").
client.onkeylog = mustCallAtLeast((line) => {
  assert.strictEqual(typeof line, 'string');
  assert.match(line, /^\S+ [0-9a-f]+ [0-9a-f]+$/i);
  gotKeylog.resolve();
});

assert.strictEqual(getDTLSSessionState(client).hasKeylogListener, true);

await client.opened;
await gotKeylog.promise;

// Clearing the listener puts the gate back.
client.onkeylog = null;
assert.strictEqual(getDTLSSessionState(client).hasKeylogListener, false);

await client.close();
await server.close();

// A session that never gets a listener never reports one, and handshakes
// normally regardless.
{
  const quietServer = listen(mustCall(), {
    cert, key, port: 0, host: '127.0.0.1',
  });
  const quietClient = connect('127.0.0.1', quietServer.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  assert.strictEqual(quietClient.onkeylog, undefined);
  assert.strictEqual(getDTLSSessionState(quietClient).hasKeylogListener, false);

  await quietClient.opened;

  assert.strictEqual(getDTLSSessionState(quietClient).hasKeylogListener, false);

  await quietClient.close();
  await quietServer.close();
}

// An exception from onkeylog is reported to the session, like an exception
// from any other callback that runs inside the handshake.
//
// OpenSSL calls the keylog callback from ssl_log_secret while deriving the
// master secret, so it runs inside SSL_do_handshake(). There is nowhere to
// report an exception from there, and it used to reach the process as an
// uncaught exception instead. It is parked and emitted once the handshake
// returns.
{
  process.on('uncaughtException', mustNotCall('reached uncaughtException'));

  const server = listen(mustCall(), { cert, key, port: 0, host: '127.0.0.1' });
  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  const reported = Promise.withResolvers();
  client.onerror = mustCall((error) => reported.resolve(error));
  client.onkeylog = mustCall(() => {
    throw new Error('thrown from onkeylog');
  });

  const error = await reported.promise;
  assert.strictEqual(error.message, 'thrown from onkeylog');

  // The session fails rather than continuing with a handler that threw.
  await assert.rejects(client.opened, { message: 'thrown from onkeylog' });

  await server.close();

  process.removeAllListeners('uncaughtException');
}
