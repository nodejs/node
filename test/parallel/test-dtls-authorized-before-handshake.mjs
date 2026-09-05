// Flags: --experimental-dtls --no-warnings

// Test: the verification accessors are safe before the handshake completes.
//
// Reading session.authorized from the listen() callback used to segfault.
// ncrypto's verifyPeerCertificate() allows for PSK by asking
// SSL_CIPHER_get_auth_nid() what authenticated the connection, and that
// dereferences SSL_get_current_cipher() without checking it -- which is NULL
// until a cipher has been negotiated.
//
// A server session reaches JavaScript before its handshake runs, so the
// listen() callback is the first thing that can ask.

import { hasCrypto, mustCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const HOST = '127.0.0.1';
const key = (name) => fixtures.readKey(name).toString();
const cert = key('agent1-cert.pem');
const privateKey = key('agent1-key.pem');
const ca = key('ca1-cert.pem');

// Server side: read both accessors at the earliest moment they exist.
{
  const checked = Promise.withResolvers();

  const endpoint = listen(mustCall((session) => {
    // Nothing has been verified at this point. Not undefined: undefined is
    // what getVerifyError() reports for "no fault found", which would read
    // as authorized.
    assert.strictEqual(session.authorized, false);
    assert.strictEqual(session.authorizationError, 'HANDSHAKE_INCOMPLETE');
    checked.resolve();
  }), { cert, key: privateKey, host: HOST, port: 0 });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;
  await checked.promise;

  await client.close();
  await endpoint.close();
}

// Client side: readable between connect() and the handshake completing.
{
  const endpoint = listen(() => {}, {
    cert, key: privateKey, host: HOST, port: 0,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
  });

  assert.strictEqual(client.authorized, false);
  assert.strictEqual(client.authorizationError, 'HANDSHAKE_INCOMPLETE');

  await client.opened;

  await client.close();
  await endpoint.close();
}

// After the handshake the accessors report the real result, so the guard has
// not simply pinned them to a constant.
{
  const endpoint = listen(() => {}, {
    cert, key: privateKey, host: HOST, port: 0,
  });

  // Trusting the issuer: authorized, with no error.
  const trusting = connect(HOST, endpoint.address.port, {
    ca: [ca], servername: 'agent1',
  });
  await trusting.opened;
  assert.strictEqual(trusting.authorized, true);
  assert.strictEqual(trusting.authorizationError, undefined);
  await trusting.close();

  // Not trusting it: a real verification error, not the placeholder.
  const untrusting = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await untrusting.opened;
  assert.strictEqual(untrusting.authorized, false);
  assert.notStrictEqual(untrusting.authorizationError, undefined);
  assert.notStrictEqual(untrusting.authorizationError, 'HANDSHAKE_INCOMPLETE');
  await untrusting.close();

  await endpoint.close();
}

// And after close, when the handle is gone.
{
  const endpoint = listen(() => {}, {
    cert, key: privateKey, host: HOST, port: 0,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;
  await client.close();

  assert.strictEqual(client.authorized, false);
  assert.strictEqual(client.authorizationError, undefined);

  await endpoint.close();
}
