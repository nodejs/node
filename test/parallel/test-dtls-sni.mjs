// Flags: --experimental-dtls --no-warnings --expose-gc

// Test: server-side SNI, the `sni` option on listen().
//
// An endpoint could report session.servername but not act on it, so one port
// could only ever present one certificate. This follows the QUIC model: a
// declarative map of host name to identity, resolved in C++ during the
// handshake. Nothing calls into JavaScript mid-handshake, so the handshake is
// never suspended -- which matters more here than it does for TLS, because a
// suspended DTLS handshake keeps retransmitting.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dtls = await import('node:dtls');
const { connect, createSecureContext, listen } = dtls;

const key = (name) => fixtures.readKey(name).toString();

// Bounded, so a regression fails with something readable rather than hanging
// until the runner gives up.
async function within(promise, what, ms = 5000) {
  const late = Symbol('late');
  // The timer has to be cleared: left pending it holds the loop open for its
  // full duration after the race is already decided.
  let timer;
  try {
    const result = await Promise.race([
      promise,
      new Promise((resolve) => { timer = setTimeout(resolve, ms, late); }),
    ]);
    assert.notStrictEqual(result, late,
                          `${what} did not happen within ${ms}ms`);
    return result;
  } finally {
    clearTimeout(timer);
  }
}

const agent1Cert = key('agent1-cert.pem');
const agent1Key = key('agent1-key.pem');
// A second identity with a different subject, to tell them apart.
const localhostCert = key('leaf-from-intermediate-cert.pem');
const localhostKey = key('leaf-from-intermediate-key.pem');

function servedCommonName(session) {
  return String(session.peerX509Certificate.subject)
    .split('\n').find((line) => line.startsWith('CN='));
}

// The name selects the certificate, and values may be either an options bag
// or a DTLSSecureContext.
{
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: {
      'agent1': { cert: agent1Cert, key: agent1Key },
      'localhost': createSecureContext({
        cert: localhostCert, key: localhostKey, isServer: true,
      }),
      '*': { cert: agent1Cert, key: agent1Key },
    },
  });

  for (const [servername, expected] of [
    ['agent1', 'CN=agent1'],
    ['localhost', 'CN=localhost'],
    ['no.such.host', 'CN=agent1'],   // Falls to the wildcard.
  ]) {
    const client = connect('127.0.0.1', endpoint.address.port, {
      servername, rejectUnauthorized: false,
    });
    await client.opened;
    assert.strictEqual(servedCommonName(client), expected);
    await client.close();
  }

  await endpoint.close();
}

// Without a wildcard, an unmatched name is refused rather than falling back
// to the endpoint's own certificate.
{
  // The session reaches onsession before SNI selection runs -- the server
  // builds it once DTLSv1_listen() has validated the cookie, and only then
  // drives the handshake that consults the map. So a refused name still
  // surfaces a session here, which then fails, exactly as any other
  // handshake failure does.
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: { 'agent1': { cert: agent1Cert, key: agent1Key } },
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'not.configured', rejectUnauthorized: false,
  });

  await assert.rejects(client.opened, (err) => {
    assert.match(err.message, /unrecognized[ _]name/);
    return true;
  });

  await endpoint.close();
}

// A client that sends no SNI at all also lands on the wildcard, and is
// refused when there is none.
{
  const withWildcard = listen(() => {}, {
    cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0,
    sni: { '*': { cert: agent1Cert, key: agent1Key } },
  });

  // servername: '' disables the extension entirely.
  const ok = connect('127.0.0.1', withWildcard.address.port, {
    servername: '', rejectUnauthorized: false,
  });
  await ok.opened;
  assert.strictEqual(servedCommonName(ok), 'CN=agent1');
  await ok.close();
  await withWildcard.close();

  const withoutWildcard = listen(() => {}, {
    cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0,
    sni: { 'agent1': { cert: agent1Cert, key: agent1Key } },
  });

  const refused = connect('127.0.0.1', withoutWildcard.address.port, {
    servername: '', rejectUnauthorized: false,
  });
  await assert.rejects(refused.opened, { name: 'Error' });
  await withoutWildcard.close();
}

// Verification follows the selected identity. SSL_set_SSL_CTX() reassigns
// ssl->ctx and the verify store is read through it, so an identity that
// trusts only ca2 rejects a client presenting a ca1 certificate even though
// the endpoint itself trusts ca1.
{
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    ca: [key('ca1-cert.pem')],
    requestCert: true,
    rejectUnauthorized: true,
    host: '127.0.0.1',
    port: 0,
    sni: {
      '*': {
        cert: agent1Cert, key: agent1Key, ca: [key('ca1-cert.pem')],
      },
      'ca2-only': {
        cert: agent1Cert, key: agent1Key, ca: [key('ca2-cert.pem')],
      },
    },
  });

  // ca1 client against the ca1 identity: accepted.
  const accepted = connect('127.0.0.1', endpoint.address.port, {
    servername: 'anything',
    cert: agent1Cert, key: agent1Key,
    rejectUnauthorized: false,
  });
  await accepted.opened;
  await accepted.close();

  // The same client against the ca2-only identity: rejected.
  const rejected = connect('127.0.0.1', endpoint.address.port, {
    servername: 'ca2-only',
    cert: agent1Cert, key: agent1Key,
    rejectUnauthorized: false,
  });
  await assert.rejects(rejected.opened, { name: 'Error' });

  // A ca2 client against the ca2-only identity: accepted.
  const other = connect('127.0.0.1', endpoint.address.port, {
    servername: 'ca2-only',
    cert: key('agent3-cert.pem'), key: key('agent3-key.pem'),
    rejectUnauthorized: false,
  });
  await other.opened;
  await other.close();

  await endpoint.close();
}

// Validation.
{
  const base = { cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0 };

  assert.throws(() => listen(mustNotCall(), { ...base, sni: {} }),
                { code: 'ERR_INVALID_ARG_VALUE' });

  for (const sni of ['x', 42, null]) {
    assert.throws(() => listen(mustNotCall(), { ...base, sni }),
                  { code: 'ERR_INVALID_ARG_TYPE' });
  }

  for (const entry of ['x', 42, null]) {
    assert.throws(() => listen(mustNotCall(), {
      ...base, sni: { 'a.example': entry },
    }), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  // A client context cannot serve as an identity.
  assert.throws(() => listen(mustNotCall(), {
    ...base,
    sni: { 'a.example': createSecureContext({ ca: [key('ca1-cert.pem')] }) },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// A callback may be given instead of a map, for identities that are chosen
// rather than enumerated. It returns the same two things a map entry holds.
{
  const prepared = createSecureContext({
    cert: localhostCert, key: localhostKey, isServer: true,
  });

  const seen = [];
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: (servername) => {
      seen.push(servername);
      if (servername === 'localhost') return prepared;
      if (servername === 'agent1') return { cert: agent1Cert, key: agent1Key };
      return undefined;
    },
  });

  for (const [servername, expected] of [
    ['agent1', 'CN=agent1'],            // An options bag.
    ['localhost', 'CN=localhost'],      // A prepared context.
  ]) {
    const client = connect('127.0.0.1', endpoint.address.port, {
      rejectUnauthorized: false, servername,
    });
    await client.opened;
    assert.strictEqual(servedCommonName(client), expected);
    await client.close();
  }

  // Declining is how a callback says a name is not served. It is refused the
  // same way an unmatched map with no wildcard is, rather than quietly
  // falling back to the endpoint's own certificate.
  const refused = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, servername: 'no.such.host',
  });
  await assert.rejects(refused.opened, { name: 'Error' });

  assert.deepStrictEqual(seen, ['agent1', 'localhost', 'no.such.host']);

  await endpoint.close();
}

// A client that sends no SNI extension still reaches the callback, and is
// told the name is absent rather than given an empty string it could not
// tell apart from one.
{
  const seen = [];
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: (servername) => {
      seen.push(servername);
      return { cert: agent1Cert, key: agent1Key };
    },
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;
  assert.deepStrictEqual(seen, [undefined]);

  await client.close();
  await endpoint.close();
}

// A context the callback built is kept alive for as long as the session
// needs it. Nothing else holds it: SSL_set_SSL_CTX() references the SSL_CTX
// and not the object wrapping it, and a callback later in the handshake
// resolves its configuration through the switched ctx and so lands on this
// context rather than the endpoint's.
//
// This exercises the path under garbage collection, but does not reliably
// fail without the retaining reference -- whether the object is collected
// inside the handshake is not something the test can force. It is here to
// exercise callback-built contexts, not as proof of the retention.
{
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    // A fresh context every time, deliberately: none of them is reachable
    // from JavaScript once the callback returns.
    sni: () => ({ cert: localhostCert, key: localhostKey }),
  });

  for (let i = 0; i < 12; i++) {
    const client = connect('127.0.0.1', endpoint.address.port, {
      rejectUnauthorized: false, servername: `host-${i}.example`,
    });
    await client.opened;
    assert.strictEqual(servedCommonName(client), 'CN=localhost');
    globalThis.gc?.();
    await client.close();
  }

  await endpoint.close();
}

// An exception from the callback fails that handshake and is reported to the
// session, like any other handshake failure. It must not reach the process as
// an uncaught exception.
{
  process.on('uncaughtException', mustNotCall());

  const thrown = new Error('from the sni callback');
  const seen = Promise.withResolvers();

  const endpoint = listen((session) => {
    session.onerror = (err) => seen.resolve(err);
  }, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: () => { throw thrown; },
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, servername: 'anything',
  });
  await assert.rejects(client.opened, { name: 'Error' });

  // The user's own error, not something wrapped around it.
  assert.strictEqual(await within(seen.promise, 'the session error'), thrown);

  await endpoint.close();
  process.removeAllListeners('uncaughtException');
}

// Validation of what a callback returns, which is checked the same way a map
// entry is.
{
  const clientContext = createSecureContext({
    cert: agent1Cert, key: agent1Key,
  });

  for (const [returned, code] of [
    [clientContext, 'ERR_INVALID_ARG_VALUE'],  // Built for a client.
    ['a string', 'ERR_INVALID_ARG_TYPE'],
    [42, 'ERR_INVALID_ARG_TYPE'],
  ]) {
    const failed = Promise.withResolvers();
    const endpoint = listen((session) => {
      session.onerror = (err) => failed.resolve(err);
    }, {
      cert: agent1Cert,
      key: agent1Key,
      host: '127.0.0.1',
      port: 0,
      sni: () => returned,
    });

    const client = connect('127.0.0.1', endpoint.address.port, {
      rejectUnauthorized: false, servername: 'anything',
    });
    await assert.rejects(client.opened, { name: 'Error' });
    assert.strictEqual(
      (await within(failed.promise, 'the session error')).code, code);

    await endpoint.close();
  }
}

// A context may appear in its own SNI map, or in a cycle with another. The
// binding holds SNI contexts weakly for this reason: a reference count cannot
// free a cycle, and these used to leak for the lifetime of the process --
// enough to trip the base_object_count_ assertion in ~Realm() at exit, which
// is what makes this test fail if the holding goes back to being strong.
{
  const registry = new FinalizationRegistry(() => { finalized++; });
  let finalized = 0;
  const total = 20;

  for (let i = 0; i < total; i++) {
    // The endpoint's own context, also serving one of its own names.
    const self = createSecureContext({
      cert: agent1Cert, key: agent1Key, isServer: true,
    });
    registry.register(self, 'self');
    const a = listen(() => {}, {
      secureContext: self,
      host: '127.0.0.1',
      port: 0,
      sni: { 'self.example': self },
    });
    await a.close();

    // Two contexts naming each other.
    const first = createSecureContext({
      cert: agent1Cert, key: agent1Key, isServer: true,
    });
    const second = createSecureContext({
      cert: agent1Cert, key: agent1Key, isServer: true,
    });
    registry.register(first, 'first');
    registry.register(second, 'second');
    const b = listen(() => {}, {
      cert: agent1Cert,
      key: agent1Key,
      host: '127.0.0.1',
      port: 0,
      sni: { 'first.example': first, 'second.example': second },
    });
    await b.close();
  }

  for (let i = 0; i < 8 && finalized < total; i++) {
    globalThis.gc();
    await new Promise((resolve) => setImmediate(resolve));
  }

  // Not all of them: whether the most recent is still reachable from a local
  // is not something a test can pin down. Most of them is the signal, and a
  // strong cycle collects none.
  assert.ok(finalized > total / 2,
            `only ${finalized}/${total} contexts were collected`);
}

// A name whose context is still configured is still served, so holding them
// weakly has not made the map unreliable.
{
  const identity = createSecureContext({
    cert: agent1Cert, key: agent1Key, isServer: true,
  });
  const server = listen(() => {}, {
    cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0,
    sni: { 'held.example': identity },
  });

  for (let i = 0; i < 4; i++) globalThis.gc();

  const client = connect('127.0.0.1', server.address.port, {
    servername: 'held.example', rejectUnauthorized: false,
  });
  await client.opened;
  await client.close();
  await server.close();
}

// Reconfiguring a context replaces its callback rather than adding to it.
//
// A map with no '*' entry refuses an unmatched name. A context that had been
// given a callback earlier kept it, so the refusal did not happen and the
// stale callback answered instead -- a configuration that reads as fail-closed
// behaving as fail-open.
{
  const shared = createSecureContext({
    cert: agent1Cert, key: agent1Key, isServer: true,
  });
  const identity = { cert: agent1Cert, key: agent1Key };

  let callbackCalls = 0;
  const withCallback = listen(() => {}, {
    secureContext: shared,
    host: '127.0.0.1',
    port: 0,
    sni: () => { callbackCalls++; return identity; },
  });

  // Same context, now a map with no wildcard.
  const withMap = listen(() => {}, {
    secureContext: shared,
    host: '127.0.0.1',
    port: 0,
    sni: { 'known.example': identity },
  });

  const unmatched = connect('127.0.0.1', withMap.address.port, {
    servername: 'unknown.example',
    rejectUnauthorized: false,
  });
  await assert.rejects(unmatched.opened, { name: 'Error' });
  assert.strictEqual(callbackCalls, 0);

  // The name that is in the map is still served.
  const matched = connect('127.0.0.1', withMap.address.port, {
    servername: 'known.example',
    rejectUnauthorized: false,
  });
  await matched.opened;
  await matched.close();

  await withCallback.close();
  await withMap.close();
}

// The SNI callback's return value is checked for being a context, not merely
// for being an object. Unwrapping validates the internal field count with a
// DCHECK only, so in a release build any other native wrapper would have been
// reinterpreted as a context and its SSL_CTX read out of whatever was there.
{
  const { Gzip } = await import('node:zlib');
  const nativeButNotAContext = new Gzip();

  const server = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    // A different kind of native object, not a DTLSSecureContext.
    sni: () => nativeButNotAContext,
  });

  const client = connect('127.0.0.1', server.address.port, {
    servername: 'a.example',
    rejectUnauthorized: false,
  });
  await assert.rejects(client.opened, { name: 'Error' });

  await server.close();
}
