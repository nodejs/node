// Flags: --experimental-dtls --no-warnings --expose-internals

// Test: a server is told when an incoming handshake fails.
//
// The endpoint used to drive the handshake before handing the session to
// JavaScript, so anything the first Cycle() reported was emitted to a session
// nobody had been given yet and was lost. A server saw a client fail to
// connect and heard nothing at all.

import { hasCrypto, mustCall, skip } from '../common/index.mjs';
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

// A handshake that fails on the server's own terms is reported to the
// session, and the session is handed over in time for a listener to see it.
{
  const reported = Promise.withResolvers();

  const endpoint = listen(mustCall((session) => {
    session.onerror = (err) => reported.resolve(err);
  }), {
    cert,
    key: privateKey,
    // Nothing the client below offers.
    ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
    host: HOST,
    port: 0,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
    ciphers: 'ECDHE-RSA-AES128-GCM-SHA256',
  });
  await assert.rejects(client.opened, { name: 'Error' });

  const err = await within(reported.promise,
                           'the server session error');
  assert.match(err.message, /no shared cipher/);

  await endpoint.close();
}

// The onsession callback runs before the handshake, so state set up there is
// in place for it. This is what makes the above work, and is worth pinning on
// its own: a listener attached there must not miss the handshake it belongs
// to.
{
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
    ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
    host: HOST,
    port: 0,
  });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
    ciphers: 'ECDHE-RSA-AES128-GCM-SHA256',
  });
  await assert.rejects(client.opened, { name: 'Error' });
  await within(settled.promise, 'the server session error');

  assert.deepStrictEqual(order, ['onsession', 'onerror']);

  await endpoint.close();
}

// Destroying a session from onsession, now that it runs first, must not let
// the handshake proceed on a session that is already gone.
{
  let first = true;
  const endpoint = listen(mustCall((session) => {
    // Only the first: the probe below has to be left alone.
    if (first) {
      first = false;
      session.destroy();
    }
  }, 2), { cert, key: privateKey, host: HOST, port: 0 });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
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
    rejectUnauthorized: false,
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
{
  const endpoint = listen(mustCall((session) => {
    session.onerror = () => assert.fail('handshake should have succeeded');
  }), { cert, key: privateKey, host: HOST, port: 0 });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;

  await client.close();
  await endpoint.close();
}
