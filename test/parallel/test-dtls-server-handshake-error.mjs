// Flags: --experimental-dtls --no-warnings --expose-internals

// Test: a server is told when an incoming handshake fails.
//
// The endpoint used to drive the handshake before handing the session to
// JavaScript, so anything the first Cycle() reported was emitted to a session
// nobody had been given yet and was lost. A server saw a client fail to
// connect and heard nothing at all.

import { hasCrypto, mustCall, mustNotCall, skip } from '../common/index.mjs';
import { setTimeout } from 'node:timers/promises';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dtls = await import('node:dtls');

// The state and sessions views are not public API; they are reached here the
// way node:quic's tests reach theirs.
const {
  getDTLSEndpointState,
} = (await import('internal/dtls/dtls')).default;
const { connect, listen } = dtls;

const HOST = '127.0.0.1';

// Bounded, so that a regression fails with something readable rather than
// hanging until the runner gives up.
async function within(promise, what, ms = 5000) {
  const late = Symbol('late');
  // The timer has to be cleared: left pending it holds the loop open for its
  // full duration after the race is already decided.
  const result = await Promise.race([promise, setTimeout(ms, late, { ref: false })]);
  assert.notStrictEqual(result, late,
                        `${what} did not happen within ${ms}ms`);
  return result;
}
const key = (name) => fixtures.readKey(name).toString();
const cert = key('agent1-cert.pem');
const privateKey = key('agent1-key.pem');
const ca = key('ca1-cert.pem');

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

// For each version, a handshake the server fails after it has validated the
// client's address and announced the session. Under DTLS 1.2 a cipher
// mismatch does; under DTLS 1.3 the cipher is chosen before the address is
// validated (see below), so a missing client certificate stands in.
const FAILURES = [
  {
    versions: V12,
    server: { ciphers: 'ECDHE-RSA-AES256-GCM-SHA384' },
    client: { ciphers: 'ECDHE-RSA-AES128-GCM-SHA256' },
    message: /no shared cipher/,
  },
  {
    versions: V13,
    server: { ca: [ca], requestCert: true },
    client: {},
    message: /peer did not return a certificate/,
  },
];

// A handshake that fails on the server's own terms is reported to the
// session, and the session is handed over in time for a listener to see it.
for (const { versions, server, client: clientOptions, message } of FAILURES) {
  const reported = Promise.withResolvers();

  const endpoint = listen(mustCall((session) => {
    session.onerror = (err) => reported.resolve(err);
  }), {
    cert,
    key: privateKey,
    host: HOST,
    port: 0,
    ...server,
    ...versions,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
    ...clientOptions,
    ...versions,
  });
  await assert.rejects(client.opened, { name: 'Error' });

  const err = await within(reported.promise,
                           'the server session error');
  assert.match(err.message, message);

  await endpoint.close();
}

// The onsession callback runs before the handshake, so state set up there is
// in place for it. This is what makes the above work, and is worth pinning on
// its own: a listener attached there must not miss the handshake it belongs
// to.
for (const { versions, server, client: clientOptions } of FAILURES) {
  const order = [];
  const settled = Promise.withResolvers();

  const endpoint = listen(mustCall((session) => {
    order.push('onsession');
    session.onerror = () => {
      order.push('onerror');
      settled.resolve();
    };
  }), {
    cert,
    key: privateKey,
    host: HOST,
    port: 0,
    ...server,
    ...versions,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
    ...clientOptions,
    ...versions,
  });
  await assert.rejects(client.opened, { name: 'Error' });
  await within(settled.promise, 'the server session error');

  assert.deepStrictEqual(order, ['onsession', 'onerror']);

  await endpoint.close();
}

// Under DTLS 1.3 the cipher suite is chosen from the first ClientHello,
// because the HelloRetryRequest that carries the cookie commits to it. A
// client with nothing in common is therefore refused before its address has
// been validated, and no session is created for it: the client is told by
// alert, and the server has nothing to report the failure on.
{
  const endpoint = listen(mustNotCall(), {
    cert,
    key: privateKey,
    host: HOST,
    port: 0,
    ciphers: 'TLS_AES_256_GCM_SHA384',
    ...V13,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
    ciphers: 'TLS_AES_128_GCM_SHA256',
    ...V13,
  });
  await assert.rejects(client.opened, /handshake failure/);
  assert.strictEqual(endpoint.stats.serverSessions, 0n);

  await endpoint.close();
}

// Destroying a session from onsession, now that it runs first, must not let
// the handshake proceed on a session that is already gone.
for (const versions of [V12, V13]) {
  let first = true;
  const endpoint = listen(mustCall((session) => {
    // Only the first: the probe below has to be left alone.
    if (first) {
      first = false;
      session.destroy();
    }
  }, 2), { cert, key: privateKey, host: HOST, port: 0, ...versions });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...versions,
  });

  // The client gets nothing back and gives up; what matters is that the
  // server is still standing.
  const pending = Symbol('pending');
  await Promise.race([
    client.opened.then(() => 'opened', () => 'failed'),
    setTimeout(300, pending),
  ]);

  assert.strictEqual(Number(getDTLSEndpointState(endpoint).sessionCount), 0);

  // Still serving.
  const good = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...versions,
  });
  const outcome = await Promise.race([
    good.opened.then(() => 'opened', () => 'failed'),
    setTimeout(500, pending),
  ]);
  assert.notStrictEqual(outcome, pending);

  client.destroy();
  good.destroy();
  await endpoint.close();
}

// A handshake that succeeds is unaffected.
for (const versions of [V12, V13]) {
  const endpoint = listen(mustCall((session) => {
    session.onerror = () => assert.fail('handshake should have succeeded');
  }), { cert, key: privateKey, host: HOST, port: 0, ...versions });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...versions,
  });
  await client.opened;

  await client.close();
  await endpoint.close();
}
